/*
 * g2_bw -- gate G2: bandwidth through the /dev/errandbuf mapping.
 *
 * memcpy of 1 MB from the mapping into a private buffer and back, N times each, timed with
 * CLOCK_MONOTONIC and rdtsc. WB should give several GB/s; UC gives hundreds of MB/s.
 * Also checks that the mapping is the service's buffer: writes a pattern through it and
 * has the service SETKEY from it (a wrong key id or an error means the pages differ).
 */
#define _GNU_SOURCE
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>
#include <sched.h>
#include <sys/mman.h>
#include "libErrand.h"
#include "tlskey_abi.h"

static inline uint64_t rdtsc_(void) { unsigned a, d; asm volatile("lfence\n\trdtsc" : "=a"(a), "=d"(d)); return ((uint64_t)d << 32) | a; }
static double now(void) { struct timespec ts; clock_gettime(CLOCK_MONOTONIC, &ts); return ts.tv_sec + ts.tv_nsec * 1e-9; }

static void
run(const char *what, void *dst, const void *src, size_t sz, int n)
{
	double t0 = now(); uint64_t c0 = rdtsc_();
	for (int i = 0; i < n; i++) memcpy(dst, src, sz);
	uint64_t c1 = rdtsc_(); double t1 = now();
	double bytes = (double)sz * n;
	printf("g2: %-22s %4d x %zu KB: %.3f s, %.2f GB/s (wall), %.1f B/cycle (%.0f cycles)\n", what, n, sz / 1024,
	       t1 - t0, bytes / (t1 - t0) / 1e9, bytes / (double)(c1 - c0), (double)(c1 - c0));
}

int
main(int argc, char **argv)
{
	int n = argc > 1 ? atoi(argv[1]) : 1000;
	const size_t SZ = 1024 * 1024;
	cpu_set_t set;
	unsigned long len;
	int fd;
	unsigned char *map, *priv;

	CPU_ZERO(&set); CPU_SET(0, &set); sched_setaffinity(0, sizeof(set), &set);
	mlockall(MCL_CURRENT | MCL_FUTURE);

	len = errand_call(TLSKEY_SLOT, TLSKEY_OP_INFO_LEN, 0, 0, 0);
	if ((long)len <= 0) { printf("g2: INFO_LEN failed: %ld\n", (long)len); return 1; }
	fd = open("/dev/errandbuf", O_RDWR);
	if (fd < 0) { perror("g2: open /dev/errandbuf"); return 1; }
	map = mmap(NULL, len, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	if (map == MAP_FAILED) { perror("g2: mmap"); return 1; }
	priv = aligned_alloc(4096, SZ);
	memset(priv, 0x5a, SZ);

	/* is it the service's buffer? put a key through it */
	for (int i = 0; i < 16; i++) map[4096 + i] = (unsigned char)(0x10 + i);
	long kid = (long)errand_call(TLSKEY_SLOT, TLSKEY_OP_SETKEY, 4096, 16, 0);
	printf("g2: mapped %lu KB at %p; SETKEY through the mapping -> key id %ld (%s)\n", len / 1024, map, kid,
	       kid > 0 ? "shared ok" : "FAILED");
	if (kid > 0) errand_call(TLSKEY_SLOT, TLSKEY_OPKEY(TLSKEY_OP_DELKEY, kid), 0, 0, 0);

	memcpy(priv, map, SZ); memcpy(map, priv, SZ); /* warm */
	run("read  map -> private", priv, map, SZ, n);
	run("write private -> map", map, priv, SZ, n);
	run("private -> private", priv + 0, priv, 4096, n); /* reference, same buffer (cache) */
	{
		unsigned char *p2 = aligned_alloc(4096, SZ); memset(p2, 1, SZ);
		run("private -> private 1M", p2, priv, SZ, n);
	}
	double gbps;
	{
		double t0 = now();
		for (int i = 0; i < n; i++) memcpy(priv, map, SZ);
		gbps = (double)SZ * n / (now() - t0) / 1e9;
	}
	printf("G2 RESULT: %s (%.2f GB/s read through the mapping, threshold 5)\n", gbps >= 5.0 ? "PASS" : "FAIL", gbps);
	return 0;
}
