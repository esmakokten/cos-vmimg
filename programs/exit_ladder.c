/*
 * exit_ladder -- guest-userspace driver for the w3_exit_anatomy exit ladder.
 *
 * Runs each rung from guest user mode directly, or asks exit_bench.ko to run it
 * from guest kernel mode, then prints one machine-readable line per rung:
 *
 *   EXITBENCH rung=L2 name=vmcall site=kernel mode=hot n=200000 \
 *             min=.. p50=.. p90=.. p99=.. p999=.. max=.. mean=.. stddev=..
 *
 * One line per rung, fixed key order, so results/ files diff cleanly across
 * configurations and ablate.sh can parse them without a second format.
 */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sched.h>
#include <sys/mman.h>
#include <sys/io.h>
#include <math.h>

#include "exitbench_abi.h"
#include "exitbench_rungs.h"

struct rung_info {
	const char *tag;
	const char *name;
	int user_ok;      /* can this rung run from guest user mode? */
};

static const struct rung_info rungs[RUNG_MAX] = {
	[RUNG_L0_RDTSC]        = { "L0", "rdtsc",       1 },
	[RUNG_L1_FASTPATH_MSR] = { "L1", "fastpath_msr",0 }, /* WRMSR is ring 0 */
	[RUNG_L2_VMCALL]       = { "L2", "vmcall",      1 },
	[RUNG_L3_CPUID]        = { "L3", "cpuid",       1 },
	[RUNG_L4_RDMSR]        = { "L4", "rdmsr",       0 }, /* RDMSR is ring 0 */
	[RUNG_L5_HLT]          = { "L5", "hlt",         0 },
	[RUNG_L6_PIO]          = { "L6", "pio_e9",      1 }, /* needs ioperm     */
	[RUNG_L7_MMIO]         = { "L7", "mmio_unbacked", 1 }, /* needs /dev/mem */
};

static int cmp_u64(const void *a, const void *b)
{
	uint64_t x = *(const uint64_t *)a, y = *(const uint64_t *)b;
	return (x > y) - (x < y);
}

static void report(const char *tag, const char *name, const char *site,
		   const char *mode, uint64_t *s, size_t n)
{
	double sum = 0.0, var = 0.0, mean;
	size_t i;

	for (i = 0; i < n; i++)
		sum += (double)s[i];
	mean = sum / (double)n;
	for (i = 0; i < n; i++) {
		double d = (double)s[i] - mean;
		var += d * d;
	}
	var /= (double)n;

	qsort(s, n, sizeof(*s), cmp_u64);

	printf("EXITBENCH rung=%s name=%s site=%s mode=%s n=%zu "
	       "min=%llu p50=%llu p90=%llu p99=%llu p999=%llu max=%llu "
	       "mean=%.1f stddev=%.1f\n",
	       tag, name, site, mode, n,
	       (unsigned long long)s[0],
	       (unsigned long long)s[(n * 50) / 100],
	       (unsigned long long)s[(n * 90) / 100],
	       (unsigned long long)s[(n * 99) / 100],
	       (unsigned long long)s[(n * 999) / 1000],
	       (unsigned long long)s[n - 1],
	       mean, sqrt(var));
	fflush(stdout);
}

static void pin(int cpu)
{
	cpu_set_t set;

	CPU_ZERO(&set);
	CPU_SET(cpu, &set);
	if (sched_setaffinity(0, sizeof(set), &set))
		fprintf(stderr, "warn: sched_setaffinity(%d): %s\n", cpu, strerror(errno));
}

/* ---- guest-user-mode execution ---------------------------------------- */

static volatile unsigned int *map_mmio_user(uint64_t gpa)
{
	void *p;
	int fd = open("/dev/mem", O_RDWR | O_SYNC);

	if (fd < 0) {
		fprintf(stderr,
			"note: /dev/mem unavailable (%s); run L7 with site=kernel\n",
			strerror(errno));
		return NULL;
	}
	p = mmap(NULL, 4096, PROT_READ | PROT_WRITE, MAP_SHARED, fd, (off_t)gpa);
	close(fd);
	if (p == MAP_FAILED) {
		fprintf(stderr,
			"note: mmap /dev/mem @0x%llx failed (%s); CONFIG_STRICT_DEVMEM?\n",
			(unsigned long long)gpa, strerror(errno));
		return NULL;
	}
	return (volatile unsigned int *)p;
}

static int run_user(int rung, uint64_t n, uint32_t flags, uint64_t gpa,
		    uint64_t *s)
{
	volatile unsigned int *mmio = NULL;
	volatile unsigned char *pollute = NULL;
	uint64_t i;

	if (!rungs[rung].user_ok) {
		fprintf(stderr, "rung %s cannot run from user mode\n", rungs[rung].tag);
		return -1;
	}
	if (rung == RUNG_L6_PIO && ioperm(0xe9, 1, 1)) {
		fprintf(stderr, "ioperm(0xe9): %s -- needs CONFIG_X86_IOPL_IOPERM=y\n",
			strerror(errno));
		return -1;
	}
	if (rung == RUNG_L7_MMIO) {
		mmio = map_mmio_user(gpa);
		if (!mmio)
			return -1;
	}
	if (flags & EXITBENCH_F_COLD) {
		pollute = malloc(EB_POLLUTE_BYTES);
		if (!pollute)
			return -1;
		memset((void *)pollute, 0, EB_POLLUTE_BYTES);
	}

	for (i = 0; i < n; i++) {
		uint64_t t0, t1;

		if (flags & EXITBENCH_F_COLD)
			eb_pollute(pollute, i);

		t0 = eb_tsc_begin();
		switch (rung) {
		case RUNG_L0_RDTSC:  eb_rung_l0_rdtsc();    break;
		case RUNG_L2_VMCALL: eb_rung_l2_vmcall();   break;
		case RUNG_L3_CPUID:  eb_rung_l3_cpuid();    break;
		case RUNG_L6_PIO:    eb_rung_l6_pio();      break;
		case RUNG_L7_MMIO:   eb_rung_l7_mmio(mmio); break;
		}
		t1 = eb_tsc_end();
		s[i] = t1 - t0;
	}

	free((void *)pollute);
	return 0;
}

/* ---- guest-kernel-mode execution via exit_bench.ko -------------------- */

static int run_kernel(int fd, int rung, uint64_t n, uint32_t flags,
		      uint64_t gpa, uint64_t chunk, uint64_t *s)
{
	struct exitbench_req req;

	memset(&req, 0, sizeof(req));
	req.rung     = (uint32_t)rung;
	req.flags    = flags;
	req.n        = n;
	req.chunk    = chunk;
	req.mmio_gpa = gpa;
	req.samples  = (uint64_t)(uintptr_t)s;

	if (ioctl(fd, EXITBENCH_RUN, &req) < 0) {
		fprintf(stderr, "ioctl(rung %s): %s\n", rungs[rung].tag, strerror(errno));
		return -1;
	}
	if (rung == RUNG_L7_MMIO)
		fprintf(stderr, "note: L7 used gpa 0x%llx\n",
			(unsigned long long)req.mmio_gpa_used);
	return 0;
}

/* ----------------------------------------------------------------------- */

static void usage(const char *p)
{
	fprintf(stderr,
"usage: %s [options]\n"
"  --rungs L0,L2,L6,...   rungs to run (default L0,L1,L2,L3,L4,L6,L7)\n"
"  --site kernel|user     where the rung executes (default kernel)\n"
"  --n N                  samples per rung (default 200000)\n"
"  --chunk N              samples per irq-disabled window, kernel site (default 1024)\n"
"  --cold                 pollute caches and branch predictors between samples\n"
"  --mmio-gpa 0xADDR      L7 target; 0 lets the module pick above RAM\n"
"  --cpu N                pin to this vCPU (default 0)\n"
"\n"
"Every line of output starts with EXITBENCH and is parsed by host/run.sh.\n", p);
}

int main(int argc, char **argv)
{
	const char *rungspec = "L0,L1,L2,L3,L4,L6,L7";
	const char *site = "kernel";
	uint64_t n = 200000, chunk = 0, gpa = 0;
	uint32_t flags = 0;
	int cpu = 0, fd = -1, i, rc = 0;
	uint64_t *s;

	for (i = 1; i < argc; i++) {
		if (!strcmp(argv[i], "--rungs") && i + 1 < argc)      rungspec = argv[++i];
		else if (!strcmp(argv[i], "--site") && i + 1 < argc)  site = argv[++i];
		else if (!strcmp(argv[i], "--n") && i + 1 < argc)     n = strtoull(argv[++i], NULL, 0);
		else if (!strcmp(argv[i], "--chunk") && i + 1 < argc) chunk = strtoull(argv[++i], NULL, 0);
		else if (!strcmp(argv[i], "--mmio-gpa") && i + 1 < argc) gpa = strtoull(argv[++i], NULL, 0);
		else if (!strcmp(argv[i], "--cpu") && i + 1 < argc)   cpu = atoi(argv[++i]);
		else if (!strcmp(argv[i], "--cold"))                  flags |= EXITBENCH_F_COLD;
		else { usage(argv[0]); return 1; }
	}

	pin(cpu);
	/* Faults inside a timed interval would be recorded as exit cost. */
	if (mlockall(MCL_CURRENT | MCL_FUTURE))
		fprintf(stderr, "warn: mlockall: %s\n", strerror(errno));

	s = aligned_alloc(64, n * sizeof(uint64_t));
	if (!s) { perror("aligned_alloc"); return 1; }
	memset(s, 0, n * sizeof(uint64_t));   /* fault it in before timing */

	if (!strcmp(site, "kernel")) {
		fd = open(EXITBENCH_DEVICE, O_RDWR);
		if (fd < 0) {
			fprintf(stderr, "open %s: %s (is exit_bench.ko loaded?)\n",
				EXITBENCH_DEVICE, strerror(errno));
			return 1;
		}
	}

	for (i = 0; i < RUNG_MAX; i++) {
		char needle[8];
		int r;

		if (!rungs[i].tag)
			continue;
		snprintf(needle, sizeof(needle), "%s", rungs[i].tag);
		if (!strstr(rungspec, needle))
			continue;

		if (!strcmp(site, "kernel"))
			r = run_kernel(fd, i, n, flags, gpa, chunk, s);
		else
			r = run_user(i, n, flags, gpa, s);

		if (r) { rc = 1; continue; }
		report(rungs[i].tag, rungs[i].name, site,
		       (flags & EXITBENCH_F_COLD) ? "cold" : "hot", s, n);
	}

	if (fd >= 0)
		close(fd);
	free(s);
	printf("EXITBENCH done rc=%d\n", rc);
	return rc;
}
