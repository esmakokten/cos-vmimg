/*
 * g1_selftest -- gate G1: AES-NI inside the service when entered by an errand from ring 3.
 *
 * Issues TLSKEY_OP_SELFTEST 1,000 times, printing this process's own XMM0 and MXCSR before
 * and after the first call, counting result and MXCSR mismatches; repeats with a non-default
 * MXCSR (FTZ|DAZ) to see whether the service restores the caller's MXCSR or the kernel's
 * lazy-FPU path replaces it; checks the argument order (ARGS); and times a null errand
 * (slot 0 pong_args), a trivial tlskey op (INFO_LEN: scrub + MXCSR restore, no cipher) and
 * SELFTEST, so a per-errand #NM would show as INFO_LEN - pong_args.
 *
 * Timing: lfence; rdtsc before, rdtscp; lfence after, nothing serializes with cpuid inside
 * the loops. Nested-QEMU numbers are for function only.
 */
#define _GNU_SOURCE
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sched.h>
#include <sys/mman.h>
#include "libErrand.h"
#include "tlskey_abi.h"
#include "stats.h"

static inline uint64_t t_begin(void) { unsigned a, d; asm volatile("lfence\n\trdtsc" : "=a"(a), "=d"(d)); return ((uint64_t)d << 32) | a; }
static inline uint64_t t_end(void)   { unsigned a, d, c; asm volatile("rdtscp\n\tlfence" : "=a"(a), "=d"(d), "=c"(c)); return ((uint64_t)d << 32) | a; }

static inline unsigned int mxcsr_get(void) { unsigned int m; asm volatile("stmxcsr %0" : "=m"(m)); return m; }
static inline void mxcsr_set(unsigned int m) { asm volatile("ldmxcsr %0" : : "m"(m)); }
static inline void xmm0_set(const unsigned char *p) { asm volatile("movdqu %0, %%xmm0" : : "m"(*(const unsigned char (*)[16])p) : "xmm0"); }
static inline void xmm0_get(unsigned char *p) { asm volatile("movdqu %%xmm0, %0" : "=m"(*(unsigned char (*)[16])p) : : "memory"); }

static void hex(const char *tag, const unsigned char *p, int n) { printf("%s", tag); for (int i = 0; i < n; i++) printf("%02x", p[i]); printf("\n"); }

static double median_of(unsigned long slot, unsigned long op, int n, uint64_t *buf)
{
	stats_t st;
	stats_init(&st, buf, n);
	for (int i = 0; i < n; i++) {
		uint64_t t0 = t_begin();
		errand_call(slot, op, 10, 20, 30);
		uint64_t t1 = t_end();
		stats_add_sample(&st, t1 - t0);
	}
	return stats_median(&st);
}

int
main(void)
{
	const int N = 1000, NT = 10000;
	unsigned char pat[16], x0[16];
	unsigned int m_before, m_after;
	int bad_ret = 0, bad_mx = 0, bad_mx_alt = 0, faults = 0;
	long fault;
	unsigned long r;
	cpu_set_t set;
	uint64_t *tbuf;

	CPU_ZERO(&set); CPU_SET(0, &set); sched_setaffinity(0, sizeof(set), &set);
	mlockall(MCL_CURRENT | MCL_FUTURE);
	for (int i = 0; i < 16; i++) pat[i] = 0xA0 + i;

	printf("g1: buffer gpa %#lx len %lu\n", errand_call(TLSKEY_SLOT, TLSKEY_OP_INFO_GPA, 0, 0, 0),
	       errand_call(TLSKEY_SLOT, TLSKEY_OP_INFO_LEN, 0, 0, 0));

	/* argument order */
	r = errand_call(TLSKEY_SLOT, TLSKEY_OP_ARGS, 1, 2, 3);
	printf("g1: ARGS(1,2,3) -> %#lx (expect %#lx) %s\n", r, 1UL | (2UL << 20) | (3UL << 40),
	       r == (1UL | (2UL << 20) | (3UL << 40)) ? "ok" : "WRONG ORDER");

	/* first call, verbose */
	m_before = mxcsr_get();
	xmm0_set(pat);
	r = errand_call_f(ERRAND_CAP(TLSKEY_SLOT), TLSKEY_OP_SELFTEST, 0, 0, 0, &fault);
	xmm0_get(x0);
	m_after = mxcsr_get();
	printf("g1: SELFTEST -> %ld fault %ld\n", (long)r, fault);
	hex("g1: xmm0 before: ", pat, 16);
	hex("g1: xmm0 after:  ", x0, 16);
	printf("g1: mxcsr before %#x after %#x\n", m_before, m_after);
	if (r) bad_ret++;
	if (fault) faults++;
	if (m_after != m_before) bad_mx++;

	for (int i = 1; i < N; i++) {
		m_before = mxcsr_get();
		xmm0_set(pat);
		r = errand_call_f(ERRAND_CAP(TLSKEY_SLOT), TLSKEY_OP_SELFTEST, 0, 0, 0, &fault);
		xmm0_get(x0);
		if (r) bad_ret++;
		if (fault) faults++;
		if (mxcsr_get() != m_before) bad_mx++;
	}
	printf("g1: %d calls: wrong result %d, faults %d, mxcsr changed %d (default mxcsr %#x)\n", N, bad_ret, faults, bad_mx, m_before);

	/* non-default MXCSR: FTZ (bit 15) | DAZ (bit 6) on top of the default mask */
	{
		unsigned int alt = 0x1f80 | 0x8000 | 0x40, seen = 0;
		for (int i = 0; i < N; i++) {
			mxcsr_set(alt);
			errand_call(TLSKEY_SLOT, TLSKEY_OP_SELFTEST, 0, 0, 0);
			seen = mxcsr_get();
			if (seen != alt) bad_mx_alt++;
		}
		printf("g1: %d calls with mxcsr %#x: came back different %d times (last seen %#x)\n", N, alt, bad_mx_alt, seen);
		mxcsr_set(0x1f80);
	}

	/* timing */
	tbuf = aligned_alloc(64, NT * sizeof(uint64_t));
	double m_pong = median_of(0, 10, NT, tbuf);
	double m_info = median_of(TLSKEY_SLOT, TLSKEY_OP_INFO_LEN, NT, tbuf);
	double m_self = median_of(TLSKEY_SLOT, TLSKEY_OP_SELFTEST, NT, tbuf);
	double m_pong2 = median_of(0, 10, NT, tbuf);
	printf("g1: medians over %d: pong_args(slot0) %.0f, INFO_LEN(slot1) %.0f, SELFTEST(slot1) %.0f, pong_args again %.0f cycles\n",
	       NT, m_pong, m_info, m_self, m_pong2);
	printf("g1: INFO_LEN - pong_args = %.0f cycles (the scrub, the MXCSR restore, and any #NM); SELFTEST - INFO_LEN = %.0f (the cipher)\n",
	       m_info - m_pong, m_self - m_info);

	printf("G1 RESULT: %s (tag %s, faults %d, mxcsr-default-changed %d, mxcsr-alt-changed %d)\n",
	       (bad_ret == 0 && faults == 0 && bad_mx == 0) ? "PASS" : "FAIL",
	       bad_ret == 0 ? "ok" : "WRONG", faults, bad_mx, bad_mx_alt);
	return 0;
}
