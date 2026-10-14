/*
 * tlsbench_raw -- the Phase 1 record-size sweep.
 *
 * Two arms, same operation: AES-128-GCM over one TLS-record-sized payload with a 13-byte AAD
 * (the TLS 1.2 record header) and a 12-byte nonce that counts up per record.
 *   in-guest: OpenSSL 3.0.17's EVP AES-GCM linked into this program -- the library, the
 *             implementation and the per-record call sequence the service runs (tlskey.c's
 *             gcm_record) -- in place on a local buffer (the baseline: same code, no boundary).
 *   errand:   the record written into the shared buffer at off_in = 0 as
 *             [nonce][aad_len][aad][payload], then one SEAL (or OPEN) errand with
 *             off_out = the payload's own offset, so the service transforms in place.
 * Sizes 256 B, 1 KB, 4 KB, 16 KB; SEAL and OPEN; 100,000 samples each; p50/p99/min.
 *
 * Timing: lfence; rdtsc before, rdtscp; lfence after. Nothing serializes with cpuid inside
 * the loops. Raw TSC cycles. Nested-QEMU numbers are for function only.
 *
 * Correctness first: the errand's SEAL of NIST GCM test case 2 is compared with the known
 * ciphertext and tag, then each arm OPENs what the other SEALed.
 *
 * `tlsbench_raw cold` (w7, added; the default run above is unchanged): the cold-footprint
 * variant. Per cell, COLD_N warm samples (the loops below) and COLD_N cold samples, each cold
 * sample preceded -- outside the timed window -- by a read-stream over a 64 MB buffer (every
 * 64 B line, 16,384 pages: past L2, L3 and the STLB, for the guest and the service alike, which
 * share the core), then the record's header written and its payload re-read (in situ the
 * provider has just copied them, so they are warm), then the same lfence/rdtsc window around
 * the one call. Reads, not writes: a write stream leaves dirty lines whose write-backs leak
 * into the timed window (results/check-cold/, 4 KB rows).
 */
#define _GNU_SOURCE
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sched.h>
#include <sys/mman.h>
#include <openssl/evp.h>
#include <openssl/err.h>
#include <openssl/crypto.h>
#include "libErrand.h"
#include "tlskey_abi.h"

#define NSAMPLES 100000
#define AAD_LEN 13

static inline uint64_t t_begin(void) { unsigned a, d; asm volatile("lfence\n\trdtsc" : "=a"(a), "=d"(d)); return ((uint64_t)d << 32) | a; }
static inline uint64_t t_end(void)   { unsigned a, d, c; asm volatile("rdtscp\n\tlfence" : "=a"(a), "=d"(d), "=c"(c)); return ((uint64_t)d << 32) | a; }

static int cmp_u64(const void *a, const void *b) { uint64_t x = *(const uint64_t *)a, y = *(const uint64_t *)b; return x < y ? -1 : x > y; }

struct pct { uint64_t min, p50, p99, max; };
static struct pct
percentiles(uint64_t *s, int n)
{
	struct pct p;
	qsort(s, n, sizeof(*s), cmp_u64);
	p.min = s[0]; p.p50 = s[n / 2]; p.p99 = s[(int)(n * 0.99)]; p.max = s[n - 1];
	return p;
}

/* in-guest arm: one ctx keyed once, as one service key slot */
static EVP_CIPHER_CTX *g_ctx;
static unsigned char   g_key[16];
extern unsigned int    OPENSSL_ia32cap_P[4]; /* libcrypto's dispatch */

static void
nonce_of(unsigned char *nonce, uint64_t seq)
{
	memset(nonce, 0, 12);
	for (int i = 0; i < 8; i++) nonce[4 + i] = (unsigned char)(seq >> (56 - 8 * i));
}

/* The service's gcm_record: CipherInit(nonce, direction), [OPEN] SET_TAG, AAD, payload in place,
 * CipherFinal, [SEAL] GET_TAG. Returns 1, or 0 (OPEN: tag mismatch). */
static int
gcm_record(int enc, const unsigned char *nonce, const unsigned char *aad, unsigned char *m, size_t len, unsigned char *tag)
{
	unsigned char fin[16];
	int outl;

	if (!EVP_CipherInit_ex(g_ctx, NULL, NULL, NULL, nonce, enc)) return 0;
	if (!enc && !EVP_CIPHER_CTX_ctrl(g_ctx, EVP_CTRL_AEAD_SET_TAG, 16, tag)) return 0;
	if (!EVP_CipherUpdate(g_ctx, NULL, &outl, aad, AAD_LEN)) return 0;
	if (len && (!EVP_CipherUpdate(g_ctx, m, &outl, m, (int)len) || (size_t)outl != len)) return 0;
	if (!EVP_CipherFinal_ex(g_ctx, fin, &outl)) { ERR_clear_error(); return 0; }
	if (enc && !EVP_CIPHER_CTX_ctrl(g_ctx, EVP_CTRL_AEAD_GET_TAG, 16, tag)) return 0;

	return 1;
}

static void
inguest_seal(unsigned char *payload, size_t len, const unsigned char *nonce, const unsigned char *aad)
{
	gcm_record(1, nonce, aad, payload, len, payload + len);
}

static int
inguest_open(unsigned char *payload, size_t len, const unsigned char *nonce, const unsigned char *aad)
{
	return gcm_record(0, nonce, aad, payload, len, payload + len) ? 0 : -1;
}

/* errand arm: the record sits at map + 0, payload at map + HDR + AAD_LEN */
static unsigned char *map;
static unsigned long  map_len;
static unsigned long  kid;
#define PAYLOAD_OFF (TLSKEY_HDR_LEN + AAD_LEN)

static inline long
errand_seal(size_t len)
{
	return (long)errand_call(TLSKEY_SLOT, TLSKEY_OPKEY(TLSKEY_OP_SEAL, kid), 0, PAYLOAD_OFF + len, PAYLOAD_OFF);
}
static inline long
errand_open(size_t len)
{
	return (long)errand_call(TLSKEY_SLOT, TLSKEY_OPKEY(TLSKEY_OP_OPEN, kid), 0, PAYLOAD_OFF + len + TLSKEY_TAG_LEN, PAYLOAD_OFF);
}
static void
record_hdr(const unsigned char *nonce, const unsigned char *aad)
{
	memcpy(map, nonce, 12);
	map[12] = AAD_LEN; map[13] = 0;
	memcpy(map + TLSKEY_HDR_LEN, aad, AAD_LEN);
}

static int
correctness(void)
{
	static const unsigned char tc2_c[16] = {0x03,0x88,0xda,0xce,0x60,0xb6,0xa3,0x92,0xf3,0x28,0xc2,0xb9,0x71,0xb2,0xfe,0x78};
	static const unsigned char tc2_t[16] = {0xab,0x6e,0x47,0xd4,0x2c,0xec,0x13,0xbd,0xf5,0x3a,0x67,0xb2,0x12,0x57,0xbd,0xdf};
	unsigned char zero[16] = {0}, nonce[12], aad[AAD_LEN], local[4096 + 16];
	long r, zk;
	int ok = 1;

	/* NIST test case 2 through the errand: key 0, nonce 0, no aad, 16 zero bytes */
	memset(map, 0, 64 + 16);
	zk = (long)errand_call(TLSKEY_SLOT, TLSKEY_OP_SETKEY, 64, 16, 0);
	if (zk <= 0) { printf("sweep: SETKEY(zero) failed %ld\n", zk); return 0; }
	memset(map, 0, 14 + 16 + 16); /* nonce 0, aad_len 0, payload 16 zero bytes */
	r = (long)errand_call(TLSKEY_SLOT, TLSKEY_OPKEY(TLSKEY_OP_SEAL, zk), 0, 14 + 16, 14);
	if (r || memcmp(map + 14, tc2_c, 16) || memcmp(map + 30, tc2_t, 16)) {
		printf("sweep: errand SEAL of GCM test case 2 WRONG (ret %ld)\n", r); ok = 0;
	} else printf("sweep: errand SEAL of GCM test case 2 ok\n");
	r = (long)errand_call(TLSKEY_SLOT, TLSKEY_OPKEY(TLSKEY_OP_OPEN, zk), 0, 14 + 32, 14);
	if (r || memcmp(map + 14, zero, 16)) { printf("sweep: errand OPEN of test case 2 WRONG (ret %ld)\n", r); ok = 0; }
	else printf("sweep: errand OPEN of GCM test case 2 ok\n");
	/* a corrupted tag must be rejected and the output zeroed */
	memset(map + 14, 0, 16); memcpy(map + 14, tc2_c, 16); memcpy(map + 30, tc2_t, 16); map[30] ^= 1;
	r = (long)errand_call(TLSKEY_SLOT, TLSKEY_OPKEY(TLSKEY_OP_OPEN, zk), 0, 14 + 32, 14);
	if (r != TLSKEY_EBADMSG) { printf("sweep: corrupted tag accepted (ret %ld)\n", r); ok = 0; }
	else printf("sweep: corrupted tag rejected (ret %ld) ok\n", r);
	/* bounds: a record past the end of the buffer */
	r = (long)errand_call(TLSKEY_SLOT, TLSKEY_OPKEY(TLSKEY_OP_SEAL, zk), map_len - 8, 64, 0);
	if (r != TLSKEY_ERANGE) { printf("sweep: out-of-range record accepted (ret %ld)\n", r); ok = 0; }
	else printf("sweep: out-of-range record rejected (ret %ld) ok\n", r);
	errand_call(TLSKEY_SLOT, TLSKEY_OPKEY(TLSKEY_OP_DELKEY, zk), 0, 0, 0);

	/* cross-check the two arms with the sweep key: errand seals, in-guest opens; and back */
	nonce_of(nonce, 7); for (int i = 0; i < AAD_LEN; i++) aad[i] = (unsigned char)i;
	for (int i = 0; i < 4096; i++) local[i] = (unsigned char)(i * 7);
	record_hdr(nonce, aad); memcpy(map + PAYLOAD_OFF, local, 4096);
	r = errand_seal(4096);
	if (r) { printf("sweep: errand SEAL failed %ld\n", r); return 0; }
	memcpy(local, map + PAYLOAD_OFF, 4096 + 16);
	if (inguest_open(local, 4096, nonce, aad)) { printf("sweep: in-guest OPEN of the errand's SEAL FAILED\n"); ok = 0; }
	else {
		int same = 1; for (int i = 0; i < 4096; i++) if (local[i] != (unsigned char)(i * 7)) same = 0;
		printf("sweep: in-guest OPEN of the errand's SEAL %s\n", same ? "ok" : "WRONG PLAINTEXT"); ok &= same;
	}
	nonce_of(nonce, 8); for (int i = 0; i < 4096; i++) local[i] = (unsigned char)(i * 3);
	inguest_seal(local, 4096, nonce, aad);
	record_hdr(nonce, aad); memcpy(map + PAYLOAD_OFF, local, 4096 + 16);
	r = errand_open(4096);
	{
		int same = 1; for (int i = 0; i < 4096; i++) if (map[PAYLOAD_OFF + i] != (unsigned char)(i * 3)) same = 0;
		printf("sweep: errand OPEN of the in-guest SEAL %s (ret %ld)\n", (r == 0 && same) ? "ok" : "FAILED", r);
		ok &= (r == 0 && same);
	}
	return ok;
}

/* --- the cold-footprint variant (`tlsbench_raw cold`) --- */
#define COLD_N   10000
#define EVICT_SZ (64UL << 20)
static unsigned char         *evb;
static volatile unsigned long sink;

static void
evict(void)
{
	unsigned long s = 0;

	for (unsigned long i = 0; i < EVICT_SZ; i += 64) s += evb[i];
	sink += s;
}

static void
touch(const unsigned char *p, size_t n)
{
	unsigned long s = 0;

	for (size_t i = 0; i < n; i += 64) s += p[i];
	sink += s;
}

/* One cell: COLD_N warm then COLD_N cold samples of one op on one arm. Returns 0, or -1 on a failed op. */
static int
cold_cell(size_t len, int open, int errand, unsigned char *local, const unsigned char *aad, uint64_t *seq,
	  uint64_t *samples, struct pct *warm, struct pct *cold)
{
	unsigned char nonce[12];

	for (int c = 0; c < 2; c++) {
		for (int i = 0; i < COLD_N; i++) {
			long r;
			nonce_of(nonce, ++*seq);
			if (errand) {
				record_hdr(nonce, aad);
				if (open) errand_seal(len);
			} else if (open) inguest_seal(local, len, nonce, aad);
			if (c) { /* cold: evict, then make the record warm as in situ */
				evict();
				if (errand) { record_hdr(nonce, aad); touch(map, PAYLOAD_OFF + len + TLSKEY_TAG_LEN); }
				else touch(local, len + 16);
			}
			uint64_t t0 = t_begin();
			if (errand) r = open ? errand_open(len) : errand_seal(len);
			else if (open) r = inguest_open(local, len, nonce, aad);
			else { inguest_seal(local, len, nonce, aad); r = 0; }
			uint64_t t1 = t_end();
			samples[i] = t1 - t0;
			if (r) { printf("cold: %s %s failed %ld at %zu\n", errand ? "errand" : "in-guest", open ? "OPEN" : "SEAL", r, len); return -1; }
		}
		*(c ? cold : warm) = percentiles(samples, COLD_N);
	}

	return 0;
}

static int
cold_main(void)
{
	static const size_t sizes[] = {256, 1024, 4096, 16384};
	unsigned char aad[AAD_LEN], *local;
	uint64_t *samples, seq = 1UL << 40;

	evb     = aligned_alloc(4096, EVICT_SZ);
	samples = aligned_alloc(64, COLD_N * sizeof(uint64_t));
	local   = aligned_alloc(4096, 16384 + 64);
	if (!evb || !samples || !local) { printf("cold: allocation failed\n"); return 1; }
	for (unsigned long i = 0; i < EVICT_SZ; i += 4096) evb[i] = 1; /* fault it in once (mlockall) */
	for (int i = 0; i < AAD_LEN; i++) aad[i] = (unsigned char)(0x17 + i);
	for (int i = 0; i < 16384; i++) local[i] = (unsigned char)i;
	memcpy(map + PAYLOAD_OFF, local, 16384);

	printf("cold: %d warm + %d cold samples per cell, eviction %lu MB read-stream; columns: size op arm warm p50 p99 cold p50 p99 penalty p50\n",
	       COLD_N, COLD_N, EVICT_SZ >> 20);
	for (unsigned s = 0; s < sizeof(sizes) / sizeof(sizes[0]); s++) {
		for (int open = 0; open < 2; open++) {
			for (int errand = 1; errand >= 0; errand--) {
				struct pct w, c;
				if (cold_cell(sizes[s], open, errand, local, aad, &seq, samples, &w, &c)) { printf("COLD RESULT: FAIL\n"); return 1; }
				printf("COLD %6zu %s %-8s %8lu %8lu %8lu %8lu   penalty p50 %+ld\n", sizes[s], open ? "OPEN" : "SEAL",
				       errand ? "errand" : "in-guest", w.p50, w.p99, c.p50, c.p99, (long)(c.p50 - w.p50));
			}
		}
	}
	printf("cold: errands served by the service so far: %lu\n", errand_call(TLSKEY_SLOT, TLSKEY_OP_COUNTERS, 0, 0, 0));
	printf("COLD RESULT: DONE\n");

	return 0;
}

int
main(int argc, char **argv)
{
	static const size_t sizes[] = {256, 1024, 4096, 16384};
	unsigned char nonce[12], aad[AAD_LEN], *local;
	uint64_t *samples, seq = 0;
	cpu_set_t set;
	int fd;

	CPU_ZERO(&set); CPU_SET(0, &set); sched_setaffinity(0, sizeof(set), &set);
	mlockall(MCL_CURRENT | MCL_FUTURE);

	map_len = errand_call(TLSKEY_SLOT, TLSKEY_OP_INFO_LEN, 0, 0, 0);
	fd = open("/dev/errandbuf", O_RDWR);
	if (fd < 0 || (long)map_len <= 0) { perror("sweep: /dev/errandbuf"); return 1; }
	map = mmap(NULL, map_len, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	if (map == MAP_FAILED) { perror("sweep: mmap"); return 1; }

	/* the session key, derived "inside nginx" in record mode: here a fixed value */
	for (int i = 0; i < 16; i++) g_key[i] = (unsigned char)(0xC0 + i);
	if (!(g_ctx = EVP_CIPHER_CTX_new()) || !EVP_CipherInit_ex(g_ctx, EVP_aes_128_gcm(), NULL, g_key, NULL, 1)) {
		printf("sweep: in-guest key init failed\n"); return 1;
	}
	/* the in-guest arm must take the assembly path the service takes (its cos_init line) */
	printf("sweep: in-guest %s: AES-NI %u PCLMUL %u AVX %u MOVBE %u\n", OpenSSL_version(OPENSSL_VERSION),
	       (OPENSSL_ia32cap_P[1] >> 25) & 1, (OPENSSL_ia32cap_P[1] >> 1) & 1, (OPENSSL_ia32cap_P[1] >> 28) & 1, (OPENSSL_ia32cap_P[1] >> 22) & 1);
	if (!((OPENSSL_ia32cap_P[1] >> 25) & 1) || !((OPENSSL_ia32cap_P[1] >> 1) & 1)) { printf("sweep: no AES-NI/PCLMUL in the guest\n"); return 1; }
	memcpy(map + 1024 * 1024, g_key, 16);
	kid = errand_call(TLSKEY_SLOT, TLSKEY_OP_SETKEY, 1024 * 1024, 16, 0);
	memset(map + 1024 * 1024, 0, 16);
	if ((long)kid <= 0) { printf("sweep: SETKEY failed %ld\n", (long)kid); return 1; }
	printf("sweep: buffer %lu KB mapped, key id %lu, %d samples per point, AAD %d B, nonce 12 B\n", map_len / 1024, kid, NSAMPLES, AAD_LEN);

	if (!correctness()) { printf("SWEEP RESULT: FAIL (correctness)\n"); return 1; }
	if (argc > 1 && !strcmp(argv[1], "cold")) {
		int r = cold_main();
		errand_call(TLSKEY_SLOT, TLSKEY_OPKEY(TLSKEY_OP_DELKEY, kid), 0, 0, 0);
		return r;
	}

	samples = aligned_alloc(64, NSAMPLES * sizeof(uint64_t));
	local   = aligned_alloc(4096, 16384 + 64);
	for (int i = 0; i < AAD_LEN; i++) aad[i] = (unsigned char)(0x17 + i);
	for (int i = 0; i < 16384; i++) local[i] = (unsigned char)i;

	printf("sweep: cycles per record, rdtsc; columns: size op arm min p50 p99 max\n");
	for (unsigned s = 0; s < sizeof(sizes) / sizeof(sizes[0]); s++) {
		size_t len = sizes[s];
		struct pct ig_seal, ig_open, er_seal, er_open;

		/* in-guest SEAL */
		for (int i = 0; i < NSAMPLES; i++) {
			nonce_of(nonce, ++seq);
			uint64_t t0 = t_begin();
			inguest_seal(local, len, nonce, aad);
			uint64_t t1 = t_end();
			samples[i] = t1 - t0;
		}
		ig_seal = percentiles(samples, NSAMPLES);
		/* in-guest OPEN (of a record sealed under the same nonce each time) */
		for (int i = 0; i < NSAMPLES; i++) {
			nonce_of(nonce, ++seq);
			inguest_seal(local, len, nonce, aad);
			uint64_t t0 = t_begin();
			int r = inguest_open(local, len, nonce, aad);
			uint64_t t1 = t_end();
			samples[i] = t1 - t0;
			if (r) { printf("sweep: in-guest OPEN failed at %zu\n", len); return 1; }
		}
		ig_open = percentiles(samples, NSAMPLES);
		/* errand SEAL: the header is written once, the payload stays in place */
		memcpy(map + PAYLOAD_OFF, local, len);
		for (int i = 0; i < NSAMPLES; i++) {
			nonce_of(nonce, ++seq);
			record_hdr(nonce, aad);
			uint64_t t0 = t_begin();
			long r = errand_seal(len);
			uint64_t t1 = t_end();
			samples[i] = t1 - t0;
			if (r) { printf("sweep: errand SEAL failed %ld at %zu\n", r, len); return 1; }
		}
		er_seal = percentiles(samples, NSAMPLES);
		/* errand OPEN */
		for (int i = 0; i < NSAMPLES; i++) {
			nonce_of(nonce, ++seq);
			record_hdr(nonce, aad);
			errand_seal(len);
			uint64_t t0 = t_begin();
			long r = errand_open(len);
			uint64_t t1 = t_end();
			samples[i] = t1 - t0;
			if (r) { printf("sweep: errand OPEN failed %ld at %zu\n", r, len); return 1; }
		}
		er_open = percentiles(samples, NSAMPLES);

		printf("SWEEP %6zu SEAL in-guest %8lu %8lu %8lu %10lu\n", len, ig_seal.min, ig_seal.p50, ig_seal.p99, ig_seal.max);
		printf("SWEEP %6zu SEAL errand   %8lu %8lu %8lu %10lu   diff p50 %+ld\n", len, er_seal.min, er_seal.p50, er_seal.p99, er_seal.max, (long)(er_seal.p50 - ig_seal.p50));
		printf("SWEEP %6zu OPEN in-guest %8lu %8lu %8lu %10lu\n", len, ig_open.min, ig_open.p50, ig_open.p99, ig_open.max);
		printf("SWEEP %6zu OPEN errand   %8lu %8lu %8lu %10lu   diff p50 %+ld\n", len, er_open.min, er_open.p50, er_open.p99, er_open.max, (long)(er_open.p50 - ig_open.p50));
	}
	printf("sweep: errands served by the service so far: %lu\n", errand_call(TLSKEY_SLOT, TLSKEY_OP_COUNTERS, 0, 0, 0));
	errand_call(TLSKEY_SLOT, TLSKEY_OPKEY(TLSKEY_OP_DELKEY, kid), 0, 0, 0);
	printf("SWEEP RESULT: DONE\n");
	return 0;
}
