/*
 * encbench -- the guest side of the system-call bounce (w9), and its native arm.
 *
 * Bounced arm: RUN starts a program (P0-P2, enclave_progs.h) inside the enclave component at
 * errand slot 4. While the errand returns ENC_SYSCALL, this loop reads the request in page 0,
 * turns the offsets enc_ptr_args() names into addresses in its mapping of the shared buffer,
 * performs the call (the null bounce answers 0 without one), writes page 1 {seq, ret} and
 * re-invokes with RESUME. The program times each call itself; the samples are fetched with
 * RESULT and REPORT after it finishes.
 *
 * Native arm: the same enclave_progs.h compiled into this program, enc_syscall = the syscall
 * instruction, the same buffers (the same mapping), the same timing macro.
 *
 *   encbench info              GPA=... LEN=... (INFO_GPA/INFO_LEN), for errandbuf's parameters
 *   encbench census <tag>      the component's errand count, then the VMM's exit census
 *   encbench anchor [n]        pong_args (slot 0) p50/p99, timed with the programs' macro
 *   encbench run <P0|P1|P2> [n]  both arms, one row per timed call, then RESULT: PASS/FAIL
 *
 * The mapping is /dev/errandbuf (errandbuf.ko loaded with the enclave's gpa and len).
 * Nothing prints while either arm runs. Nested-QEMU numbers are for function only.
 */
#define _GNU_SOURCE
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sched.h>
#include <sys/io.h>
#include <sys/mman.h>
#include "libErrand.h"
#include "enclave_abi.h"

static unsigned char *map;
static unsigned long  map_len;

static inline long
sys6(long nr, long a0, long a1, long a2, long a3, long a4, long a5)
{
	register long r10 __asm__("r10") = a3;
	register long r8 __asm__("r8")   = a4;
	register long r9 __asm__("r9")   = a5;
	long ret;

	__asm__ __volatile__("syscall"
			     : "=a"(ret)
			     : "a"(nr), "D"(a0), "S"(a1), "d"(a2), "r"(r10), "r"(r8), "r"(r9)
			     : "rcx", "r11", "memory");

	return ret;
}

/* The native arm's enc_syscall: what the guest stub does for a bounced call, minus the bounce.
 * Not inlined, as the component's enc_syscall is a call too. */
static long __attribute__((noinline))
enc_syscall(long nr, long a0, long a1, long a2, long a3, long a4, long a5)
{
	if (nr == ENC_NR_NULL) return 0;

	return sys6(nr, a0, a1, a2, a3, a4, a5);
}

#define ENC_PTR(off) (map + (off))
#define ENC_ARG(off) ((long)(map + (off)))
static unsigned long long enc_samples[ENC_MAX_SAMPLES];
#include "enclave_progs.h"

static unsigned long long bsamp[ENC_MAX_SAMPLES]; /* the bounced arm's samples, from REPORT */

static long
errand(long op, long a, long b, long c)
{
	long fault = 0;
	long r = (long)errand_call_f(ERRAND_CAP(ENC_SLOT), op, a, b, c, &fault);

	if (fault) {
		printf("encbench: errand op %ld faulted (rax %ld): no enclave at slot %d?\n", op, r, ENC_SLOT);
		exit(2);
	}

	return r;
}

/*
 * The bounced arm: the guest stub. Returns RUN's or the last RESUME's result (ENC_DONE when
 * the program finished); *nb counts the calls performed for it.
 */
static long
bounce_run(long prog, long n, long *nb)
{
	volatile struct enc_req  *q = (volatile struct enc_req *)(map + ENC_REQ_OFF);
	volatile struct enc_resp *p = (volatile struct enc_resp *)(map + ENC_RESP_OFF);
	long r;

	*nb = 0;
	r   = errand(ENC_OP_RUN, prog, n, 0);
	while (r == ENC_SYSCALL) {
		long nr = q->nr, a[6], ret;
		unsigned int m = enc_ptr_args(nr);
		int i;

		for (i = 0; i < 6; i++) a[i] = q->a[i];
		for (i = 0; i < 6; i++) {
			if (!(m & (1u << i))) continue;
			if ((unsigned long)a[i] >= map_len) break;
			a[i] = (long)(map + a[i]);
		}
		if (i < 6) ret = -14; /* -EFAULT: an offset outside the buffer */
		else if (nr == ENC_NR_NULL) ret = 0;
		else ret = sys6(nr, a[0], a[1], a[2], a[3], a[4], a[5]);
		p->ret = ret;
		__asm__ __volatile__("" ::: "memory");
		p->seq = q->seq; /* last */
		(*nb)++;
		r = errand(ENC_OP_RESUME, ret, 0, 0);
	}

	return r;
}

static int
cmp_u64(const void *a, const void *b)
{
	unsigned long long x = *(const unsigned long long *)a, y = *(const unsigned long long *)b;

	return x < y ? -1 : x > y;
}

struct pct {
	unsigned long long min, p50, p99, max;
};

static struct pct
pct_of(unsigned long long *s, long n)
{
	struct pct p = { 0, 0, 0, 0 };

	if (n <= 0) return p;
	qsort(s, n, sizeof(*s), cmp_u64);
	p.min = s[0];
	p.p50 = s[n / 2];
	p.p99 = s[(n * 99) / 100];
	p.max = s[n - 1];

	return p;
}

static void
map_buffer(void)
{
	int fd = open("/dev/errandbuf", O_RDWR);

	map_len = (unsigned long)errand(ENC_OP_INFO_LEN, 0, 0, 0);
	if (fd < 0 || (long)map_len <= 0) {
		perror("encbench: /dev/errandbuf");
		exit(1);
	}
	map = mmap(NULL, map_len, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	if (map == MAP_FAILED) {
		perror("encbench: mmap");
		exit(1);
	}
}

/* Pin to CPU 0 and lock what is mapped now (MCL_CURRENT only: MCL_FUTURE would make every mmap
 * in P2 populate and lock its pages). */
static void
settle(void)
{
	cpu_set_t set;

	CPU_ZERO(&set);
	CPU_SET(0, &set);
	sched_setaffinity(0, sizeof(set), &set);
	memset(enc_samples, 0, sizeof(enc_samples));
	memset(bsamp, 0, sizeof(bsamp));
	if (mlockall(MCL_CURRENT)) perror("encbench: mlockall");
}

static int
cmd_run(long prog, long n)
{
	static const char *pname[ENC_NPROGS] = { "P0", "P1", "P2" };
	struct enc_result br, nr;
	long c0, c1, r, nb, got, row, ok;
	unsigned long nnat;

	map_buffer();
	settle();

	/* Bounced arm. */
	c0 = errand(ENC_OP_COUNTERS, 0, 0, 0);
	r  = bounce_run(prog, n, &nb);
	if (errand(ENC_OP_RESULT, ENC_REPORT_OFF, 0, 0)) {
		printf("encbench: RESULT failed\n");
		return 1;
	}
	memcpy(&br, map + ENC_REPORT_OFF, sizeof(br));
	got = errand(ENC_OP_REPORT, ENC_REPORT_OFF, 0, br.nsamples);
	if (got > 0) memcpy(bsamp, map + ENC_REPORT_OFF, got * sizeof(bsamp[0]));
	c1 = errand(ENC_OP_COUNTERS, 0, 0, 0);

	/* Native arm: the same program, here. */
	enc_prog_run(prog, n);
	nr   = enc_res;
	nnat = enc_nsamp;

	printf("ENCPROG %s bounced: ret %ld (%s), n %ld, rows %ld, samples %ld (fetched %ld), bounces %ld (guest saw %ld), "
	       "seq mismatches %ld, failed checks %ld (first: row %ld ret %ld)\n",
	       pname[prog], r, r == ENC_DONE ? "ENC_DONE" : "NOT DONE", br.n, br.nrows, br.nsamples, got, br.nbounces, nb,
	       br.nseq_bad, br.nfail, br.fail_row, br.fail_ret);
	/* Errands between the two COUNTERS: RUN, one RESUME per bounce, RESULT, REPORT, the second COUNTERS. */
	printf("ENCPROG %s errands: %ld counted, %ld expected (1 RUN + %ld RESUME + RESULT + REPORT + COUNTERS)\n",
	       pname[prog], c1 - c0, nb + 4, nb);
	printf("ENCPROG %s native: n %ld, samples %lu, failed checks %ld (first: row %ld ret %ld)\n", pname[prog], nr.n,
	       nnat, nr.nfail, nr.fail_row, nr.fail_ret);

	ok = r == ENC_DONE && got == br.nsamples && br.nsamples == br.n * br.nrows && br.nbounces == nb
	     && br.nseq_bad == 0 && br.nfail == 0 && c1 - c0 == nb + 4 && nr.nfail == 0 && (long)nnat == br.nsamples;

	for (row = 0; row < br.nrows && got == br.nsamples && (long)nnat == br.nsamples; row++) {
		struct pct b = pct_of(bsamp + row * br.n, br.n);
		struct pct t = pct_of(enc_samples + row * br.n, br.n);

		printf("ENCROW %s %-7s n %ld | bounced p50 %llu p99 %llu max %llu min %llu | native p50 %llu p99 %llu max %llu "
		       "min %llu | ratio %.2f | overhead %+lld | outside: %s\n",
		       pname[prog], enc_row_names[prog][row], br.n, b.p50, b.p99, b.max, b.min, t.p50, t.p99, t.max, t.min,
		       t.p50 ? (double)b.p50 / (double)t.p50 : 0.0, (long long)b.p50 - (long long)t.p50,
		       enc_row_outside[prog][row]);
	}
	printf("ENCPROG %s RESULT: %s\n", pname[prog], ok ? "PASS" : "FAIL");

	return ok ? 0 : 1;
}

static int
cmd_anchor(long n)
{
	static unsigned long long s[ENC_MAX_SAMPLES];
	struct pct p;
	long i, r = 0;

	if (n <= 0 || n > ENC_MAX_SAMPLES) n = ENC_MAX_SAMPLES;
	settle();
	for (i = 0; i < n; i++) {
		unsigned long long t0 = enc_t_begin();
		r = (long)errand_call(0, 10, 20, 30, 40);
		unsigned long long t1 = enc_t_end();
		s[i] = t1 - t0;
	}
	p = pct_of(s, n);
	printf("ANCHOR pong_args n %ld | p50 %llu p99 %llu max %llu min %llu | ret %ld (%s)\n", n, p.p50, p.p99, p.max, p.min,
	       r, r == 100 ? "ok" : "WRONG");

	return r == 100 ? 0 : 1;
}

int
main(int argc, char **argv)
{
	const char *cmd = argc > 1 ? argv[1] : "";

	if (!strcmp(cmd, "info")) {
		printf("GPA=%#lx LEN=%lu\n", (unsigned long)errand(ENC_OP_INFO_GPA, 0, 0, 0),
		       (unsigned long)errand(ENC_OP_INFO_LEN, 0, 0, 0));
		return 0;
	}
	if (!strcmp(cmd, "census")) {
		unsigned int tag = argc > 2 ? (unsigned int)strtoul(argv[2], NULL, 0) : 0;

		printf("census[%u]: enclave errands served %ld\n", tag, errand(ENC_OP_COUNTERS, 0, 0, 0));
		fflush(stdout);
		if (iopl(3)) {
			perror("iopl");
			return 1;
		}
		outb((unsigned char)tag, ENC_CENSUS_PORT);
		return 0;
	}
	if (!strcmp(cmd, "anchor")) return cmd_anchor(argc > 2 ? strtol(argv[2], NULL, 0) : 0);
	if (!strcmp(cmd, "run") && argc > 2 && argv[2][0] == 'P' && argv[2][1] >= '0' && argv[2][1] < '0' + ENC_NPROGS) {
		return cmd_run(argv[2][1] - '0', argc > 3 ? strtol(argv[3], NULL, 0) : 0);
	}
	fprintf(stderr, "usage: encbench info | census <tag> | anchor [n] | run <P0|P1|P2> [n]\n");

	return 2;
}
