/*
 * uarch_probe -- what a VM exit costs the guest *after* it returns.
 *
 * The exit ladder prices the round trip. It does not price what the round trip
 * did to the guest: the VMM's handler runs on the same caches, TLBs and branch
 * predictors, and everything it touches is something the guest has to fetch
 * again. That collateral cost is invisible to every exit benchmark that times
 * only the instruction, this project's earlier numbers included.
 *
 * Protocol, per exit type and per burst length k, interleaved so that drift in
 * either direction cancels:
 *
 *     warm the structure
 *     b = time(chase(k))          <- steady state
 *     warm the structure
 *     exit                        <- the exit under test
 *     a = time(chase(k))          <- immediately after returning
 *
 * Report median(a) - median(b). Sweeping k gives the recovery envelope: how
 * many accesses the guest runs slowly for, not just by how much.
 *
 * Claim under test: recovery cost scales with the VMM's code and data
 * footprint, so a KVM->QEMU round trip costs the guest materially more than a
 * minimal VMM whose handler fits in a few cache lines. If the deltas come back
 * near zero, the claim is dead and that is worth knowing before it reaches a
 * paper.
 */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <errno.h>
#include <sched.h>
#include <sys/mman.h>
#include <sys/io.h>
#include <fcntl.h>

#include "exitbench_abi.h"
#include "exitbench_rungs.h"

#define LINE 64

struct probe {
	const char *name;
	size_t bytes;
};

/* Sized to land in a named level of the hierarchy on Skylake-SP: 32 KB L1d,
 * 1 MB private L2, 33 MB shared LLC. 4 MB is the LLC probe -- comfortably
 * LLC-resident and still cheap enough to re-warm on every repetition, which a
 * 16 MB chain is not. */
static const struct probe probes[] = {
	{ "L1_32K",   32u << 10 },
	{ "L2_256K", 256u << 10 },
	{ "L2_1M",     1u << 20 },
	{ "LLC_4M",    4u << 20 },
};
#define NPROBES (sizeof(probes) / sizeof(probes[0]))

static const size_t bursts[] = { 16, 64, 256, 1024 };
#define NBURSTS (sizeof(bursts) / sizeof(bursts[0]))

/* A randomised cycle over one pointer per cache line: each load depends on the
 * previous one, so the measured time is latency, not bandwidth, and the
 * hardware prefetcher cannot help. */
static void **build_chain(size_t bytes)
{
	size_t n = bytes / LINE, i;
	size_t *order;
	void **buf;

	buf = aligned_alloc(4096, bytes);
	order = malloc(n * sizeof(*order));
	if (!buf || !order)
		return NULL;
	memset(buf, 0, bytes);

	for (i = 0; i < n; i++)
		order[i] = i;
	for (i = n - 1; i > 0; i--) {           /* Fisher-Yates */
		size_t j = (size_t)(random() % (long)(i + 1));
		size_t t = order[i]; order[i] = order[j]; order[j] = t;
	}
	for (i = 0; i < n; i++)
		buf[order[i] * (LINE / sizeof(void *))] =
			&buf[order[(i + 1) % n] * (LINE / sizeof(void *))];

	free(order);
	return buf;
}

/* noinline + the asm sink: without both, GCC proves the result unused and
 * deletes the whole dependent-load chain, and every burst length then reports
 * the same constant. That failure is silent -- it looks like a clean run. */
static __attribute__((noinline)) void *chase(void *p, size_t steps)
{
	size_t i;
	for (i = 0; i < steps; i++)
		p = *(void **)p;
	__asm__ __volatile__("" :: "r"(p) : "memory");
	return p;
}

static int cmp_u64(const void *a, const void *b)
{
	uint64_t x = *(const uint64_t *)a, y = *(const uint64_t *)b;
	return (x > y) - (x < y);
}

static uint64_t median(uint64_t *v, size_t n)
{
	qsort(v, n, sizeof(*v), cmp_u64);
	return v[n / 2];
}

static void do_exit_rung(int rung, volatile unsigned int *mmio)
{
	switch (rung) {
	case RUNG_L0_RDTSC:  eb_rung_l0_rdtsc();    break;
	case RUNG_L2_VMCALL: eb_rung_l2_vmcall();   break;
	case RUNG_L3_CPUID:  eb_rung_l3_cpuid();    break;
	case RUNG_L6_PIO:    eb_rung_l6_pio();      break;
	case RUNG_L7_MMIO:   eb_rung_l7_mmio(mmio); break;
	}
}

int main(int argc, char **argv)
{
	int cpu = 0, rung = RUNG_L2_VMCALL, reps = 400, pi, bi, r;
	uint64_t gpa = 0;
	const char *rname = "vmcall";
	volatile unsigned int *mmio = NULL;
	uint64_t *as, *bs;
	cpu_set_t set;
	int i;

	for (i = 1; i < argc; i++) {
		if (!strcmp(argv[i], "--cpu") && i + 1 < argc) cpu = atoi(argv[++i]);
		else if (!strcmp(argv[i], "--reps") && i + 1 < argc) reps = atoi(argv[++i]);
		else if (!strcmp(argv[i], "--mmio-gpa") && i + 1 < argc) gpa = strtoull(argv[++i], NULL, 0);
		else if (!strcmp(argv[i], "--rung") && i + 1 < argc) {
			rname = argv[++i];
			if (!strcmp(rname, "none"))   rung = RUNG_L0_RDTSC;
			else if (!strcmp(rname, "vmcall")) rung = RUNG_L2_VMCALL;
			else if (!strcmp(rname, "cpuid"))  rung = RUNG_L3_CPUID;
			else if (!strcmp(rname, "pio"))    rung = RUNG_L6_PIO;
			else if (!strcmp(rname, "mmio"))   rung = RUNG_L7_MMIO;
			else { fprintf(stderr, "unknown rung %s\n", rname); return 1; }
		} else {
			fprintf(stderr,
"usage: %s [--rung none|vmcall|cpuid|pio|mmio] [--reps N] [--cpu N] [--mmio-gpa 0xA]\n"
"  --rung none is the control: same protocol, no exit. Its delta is the\n"
"  measurement's own noise floor and must be near zero for the rest to mean\n"
"  anything.\n", argv[0]);
			return 1;
		}
	}

	CPU_ZERO(&set); CPU_SET(cpu, &set);
	sched_setaffinity(0, sizeof(set), &set);
	mlockall(MCL_CURRENT | MCL_FUTURE);
	srandom(12345);   /* same chain layout across configurations */

	if (rung == RUNG_L6_PIO && ioperm(0xe9, 1, 1)) {
		fprintf(stderr, "ioperm(0xe9): %s\n", strerror(errno));
		return 1;
	}
	if (rung == RUNG_L7_MMIO) {
		int fd = open("/dev/mem", O_RDWR | O_SYNC);
		void *p = MAP_FAILED;
		if (fd >= 0) {
			p = mmap(NULL, 4096, PROT_READ | PROT_WRITE, MAP_SHARED,
				 fd, (off_t)gpa);
			close(fd);
		}
		if (p == MAP_FAILED) {
			fprintf(stderr, "cannot map gpa 0x%llx for L7\n",
				(unsigned long long)gpa);
			return 1;
		}
		mmio = (volatile unsigned int *)p;
	}

	as = malloc(reps * sizeof(*as));
	bs = malloc(reps * sizeof(*bs));
	if (!as || !bs) return 1;

	for (pi = 0; pi < (int)NPROBES; pi++) {
		void **chain = build_chain(probes[pi].bytes);
		size_t full = probes[pi].bytes / LINE;
		void *p;

		if (!chain) {
			fprintf(stderr, "alloc failed for %s\n", probes[pi].name);
			continue;
		}

		/* Re-warming the whole chain is O(footprint) and happens twice per
		 * repetition, so large probes get proportionally fewer reps.
		 * Without this the 4 MB probe alone runs for minutes and the
		 * operator stops trusting the harness. */
		int probe_reps = reps / (1 + (int)(probes[pi].bytes >> 18));
		if (probe_reps < 32)
			probe_reps = 32;

		for (bi = 0; bi < (int)NBURSTS; bi++) {
			size_t k = bursts[bi] < full ? bursts[bi] : full;
			double da, db;

			for (r = 0; r < probe_reps; r++) {
				uint64_t t0, t1;

				p = chase((void *)chain, full);
				t0 = eb_tsc_begin();
				p = chase((void *)chain, k);
				t1 = eb_tsc_end();
				bs[r] = t1 - t0;

				p = chase((void *)chain, full);
				do_exit_rung(rung, mmio);
				t0 = eb_tsc_begin();
				p = chase((void *)chain, k);
				t1 = eb_tsc_end();
				as[r] = t1 - t0;
				(void)p;
			}

			da = (double)median(as, probe_reps);
			db = (double)median(bs, probe_reps);
			printf("UARCH rung=%s probe=%s burst=%zu "
			       "steady=%.0f after=%.0f delta=%.0f "
			       "per_access=%.2f reps=%d\n",
			       rname, probes[pi].name, k, db, da, da - db,
			       (da - db) / (double)k, probe_reps);
			fflush(stdout);
		}
		free(chain);
	}

	free(as); free(bs);
	printf("UARCH done\n");
	return 0;
}
