/*
 * signbench -- Phase 3 of w7 (cut 1): the server's ECDSA P-256 key lives only in the tlskey
 * service. Usage: signbench <cert.pem> [placeholder-key.pem]
 *
 * G-sign, in order (any failure ends the run with SIGN RESULT: FAIL):
 *   1. PUBKEY: the service's public point equals the certificate's.
 *   2. A SIGN errand over a fixed digest verifies under the certificate's public key (the default
 *      provider), and does not verify over a changed digest; malformed SIGNs are refused.
 *   3. The provider path libssl takes: a separate library context with the errand provider in
 *      TLS_ERRAND=sign mode preferred, the placeholder key loaded from its PEM, EVP_DigestSign
 *      (SHA-256) of a message, verified in the default context under the certificate -- and the
 *      provider's SIGN counter moved by one.
 *
 * The micro: one ECDSA P-256 signature of the same 32-byte digest, two arms, 10,000 samples each,
 * after 200 unrecorded warm-up calls, the window of tlsbench_raw (lfence; rdtsc ... rdtscp; lfence):
 *   in-guest: EVP_PKEY_sign with a P-256 key generated here at start (the service's key never
 *             enters the guest; the code path is the same: OpenSSL 3.0.17, one ctx, sign_init once)
 *   errand:   one SIGN errand, the digest already in the shared buffer (the service copies it out
 *             each time), the DER signature written back into the buffer.
 * Every signature of both arms is verified after the window (a wrong SIGN is counted, not timed).
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
#include <openssl/pem.h>
#include <openssl/x509.h>
#include <openssl/err.h>
#include <openssl/crypto.h>
#include <openssl/provider.h>
#include <openssl/core_names.h>
#include "libErrand.h"
#include "tlskey_abi.h"

#define NSAMPLES 10000
#define NWARM    200

extern int OSSL_provider_init_errand(const OSSL_CORE_HANDLE *, const OSSL_DISPATCH *, const OSSL_DISPATCH **, void **);
extern unsigned long errand_provider_signs(void);

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

/* the shared buffer, laid out as the provider uses it */
#define OFF_DG  1024UL
#define OFF_SIG 1088UL
static unsigned char *map;
static unsigned long  map_len;

static inline long
sign_errand(void)
{
	return (long)errand_call(TLSKEY_SLOT, TLSKEY_OP_SIGN, OFF_DG, TLSKEY_DIGEST_LEN, OFF_SIG);
}

static int
verify(EVP_PKEY_CTX *vctx, const unsigned char *sig, size_t siglen, const unsigned char *dg)
{
	int r = EVP_PKEY_verify(vctx, sig, siglen, dg, TLSKEY_DIGEST_LEN);

	ERR_clear_error();
	return r == 1;
}

/* step 3: the provider path, in its own library context */
static int
provider_path(const char *placeholder, EVP_PKEY *cert_pub)
{
	static const char msg[] = "TLS 1.3, server CertificateVerify -- stand-in transcript";
	OSSL_LIB_CTX *pl = OSSL_LIB_CTX_new();
	EVP_PKEY *ph = NULL;
	EVP_MD_CTX *md = NULL, *mv = NULL;
	unsigned char sig[TLSKEY_SIG_MAX];
	size_t siglen = sizeof(sig);
	unsigned long before;
	BIO *bio = NULL;
	int ok = 0;

	setenv("TLS_ERRAND", "sign", 1);
	if (!pl || OSSL_PROVIDER_add_builtin(pl, "errand", OSSL_provider_init_errand) != 1
	    || !OSSL_PROVIDER_load(pl, "errand") || !OSSL_PROVIDER_load(pl, "default")
	    || EVP_set_default_properties(pl, "?provider=errand") != 1) {
		printf("sign: provider registration FAILED\n"); goto done;
	}
	if (!(bio = BIO_new_file(placeholder, "r")) || !(ph = PEM_read_bio_PrivateKey_ex(bio, NULL, NULL, NULL, pl, NULL))) {
		printf("sign: placeholder key %s does not load\n", placeholder); goto done;
	}
	before = errand_provider_signs();
	if (!(md = EVP_MD_CTX_new()) || EVP_DigestSignInit_ex(md, NULL, "SHA256", pl, NULL, ph, NULL) <= 0
	    || EVP_DigestSign(md, NULL, &siglen, (const unsigned char *)msg, sizeof(msg)) <= 0
	    || EVP_DigestSign(md, sig, &siglen, (const unsigned char *)msg, sizeof(msg)) <= 0) {
		printf("sign: EVP_DigestSign through the provider FAILED\n"); ERR_print_errors_fp(stdout); goto done;
	}
	/* verified in the default context, under the certificate's key */
	if (!(mv = EVP_MD_CTX_new()) || EVP_DigestVerifyInit_ex(mv, NULL, "SHA256", NULL, NULL, cert_pub, NULL) <= 0
	    || EVP_DigestVerify(mv, sig, siglen, (const unsigned char *)msg, sizeof(msg)) != 1) {
		printf("sign: the provider's signature does NOT verify under the certificate\n"); goto done;
	}
	printf("sign: provider path (placeholder key, EVP_DigestSign SHA-256): %zu-byte signature verifies under the certificate, SIGN errands %lu -> %lu\n",
	       siglen, before, errand_provider_signs());
	ok = errand_provider_signs() == before + 1;
done:
	ERR_clear_error();
	EVP_MD_CTX_free(md);
	EVP_MD_CTX_free(mv);
	EVP_PKEY_free(ph);
	BIO_free(bio);
	unsetenv("TLS_ERRAND");
	/* pl and its providers stay loaded: freeing the context would DELKEY nothing, but tearing the
	 * provider down mid-program buys nothing either */
	return ok;
}

int
main(int argc, char **argv)
{
	const char *certf = argc > 1 ? argv[1] : "/nginx/conf/cert-sign.pem";
	const char *phf = argc > 2 ? argv[2] : "/nginx/conf/key-placeholder.pem";
	unsigned char cert_pt[TLSKEY_PUB_LEN], dg[TLSKEY_DIGEST_LEN], sig[TLSKEY_SIG_MAX], gsig[TLSKEY_SIG_MAX];
	size_t ptlen = 0, gsiglen;
	EVP_PKEY_CTX *vctx = NULL, *gctx = NULL, *gvctx = NULL;
	EVP_PKEY *cert_pub, *gkey;
	uint64_t *ig = malloc(sizeof(uint64_t) * NSAMPLES), *er = malloc(sizeof(uint64_t) * NSAMPLES);
	struct pct pi, pe;
	int bad_ig = 0, bad_er = 0, fails = 0;
	X509 *x = NULL;
	FILE *f;
	long r;
	int fd;

	cpu_set_t set; CPU_ZERO(&set); CPU_SET(0, &set); sched_setaffinity(0, sizeof(set), &set);
	if (!(f = fopen(certf, "r")) || !(x = PEM_read_X509(f, NULL, NULL, NULL)) || !(cert_pub = X509_get0_pubkey(x))
	    || !EVP_PKEY_get_octet_string_param(cert_pub, OSSL_PKEY_PARAM_PUB_KEY, cert_pt, sizeof(cert_pt), &ptlen)
	    || ptlen != TLSKEY_PUB_LEN) {
		printf("sign: cannot read the certificate's P-256 public point from %s\nSIGN RESULT: FAIL\n", certf); return 1;
	}
	map_len = errand_call(TLSKEY_SLOT, TLSKEY_OP_INFO_LEN, 0, 0, 0);
	if ((long)map_len <= 0 || (fd = open("/dev/errandbuf", O_RDWR)) < 0
	    || (map = mmap(NULL, map_len, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0)) == MAP_FAILED) {
		printf("sign: no shared buffer\nSIGN RESULT: FAIL\n"); return 1;
	}

	/* 1. the service's public point is the certificate's */
	r = (long)errand_call(TLSKEY_SLOT, TLSKEY_OP_PUBKEY, OFF_SIG, 0, 0);
	if (r != TLSKEY_PUB_LEN || memcmp(map + OFF_SIG, cert_pt, TLSKEY_PUB_LEN)) {
		printf("sign: PUBKEY -> %ld; the service's public point %s the certificate's\n", r, r == TLSKEY_PUB_LEN ? "DIFFERS FROM" : "?"); fails++;
	} else printf("sign: PUBKEY -> %ld: the service's public point is the certificate's\n", r);

	/* 2. a SIGN errand verifies under the certificate, and only over its own digest */
	for (int i = 0; i < TLSKEY_DIGEST_LEN; i++) dg[i] = (unsigned char)(0xa5 ^ i);
	memcpy(map + OFF_DG, dg, TLSKEY_DIGEST_LEN);
	vctx = EVP_PKEY_CTX_new_from_pkey(NULL, cert_pub, NULL);
	if (!vctx || EVP_PKEY_verify_init(vctx) <= 0) { printf("sign: verify ctx FAILED\nSIGN RESULT: FAIL\n"); return 1; }
	r = sign_errand();
	if (r <= 0 || r > TLSKEY_SIG_MAX || !verify(vctx, map + OFF_SIG, (size_t)r, dg)) {
		printf("sign: SIGN -> %ld, signature does NOT verify under the certificate\n", r); fails++;
	} else {
		dg[0] ^= 1;
		if (verify(vctx, map + OFF_SIG, (size_t)r, dg)) { printf("sign: SIGN's signature verifies over a CHANGED digest\n"); fails++; }
		else printf("sign: SIGN -> %ld-byte DER signature: verifies under the certificate, not over a changed digest\n", r);
		dg[0] ^= 1;
	}
	r = (long)errand_call(TLSKEY_SLOT, TLSKEY_OP_SIGN, OFF_DG, 31, OFF_SIG);
	long r2 = (long)errand_call(TLSKEY_SLOT, TLSKEY_OP_SIGN, OFF_DG, TLSKEY_DIGEST_LEN, map_len - 8);
	long r3 = (long)errand_call(TLSKEY_SLOT, TLSKEY_OP_SIGN, map_len - 8, TLSKEY_DIGEST_LEN, OFF_SIG);
	if (r != TLSKEY_EINVAL || r2 != TLSKEY_ERANGE || r3 != TLSKEY_ERANGE) { printf("sign: malformed SIGNs -> %ld %ld %ld (expect %ld %ld %ld)\n", r, r2, r3, TLSKEY_EINVAL, TLSKEY_ERANGE, TLSKEY_ERANGE); fails++; }
	else printf("sign: malformed SIGNs refused: digest length 31 -> %ld, signature past the buffer -> %ld, digest past the buffer -> %ld\n", r, r2, r3);

	/* 3. the provider path */
	if (!provider_path(phf, cert_pub)) fails++;
	printf("G-SIGN RESULT: %s\n", fails ? "FAIL" : "PASS");
	if (fails) { printf("SIGN RESULT: FAIL\n"); return 1; }

	/* the micro */
	if (!(gkey = EVP_PKEY_Q_keygen(NULL, NULL, "EC", "P-256")) || !(gctx = EVP_PKEY_CTX_new_from_pkey(NULL, gkey, NULL))
	    || EVP_PKEY_sign_init(gctx) <= 0 || !(gvctx = EVP_PKEY_CTX_new_from_pkey(NULL, gkey, NULL)) || EVP_PKEY_verify_init(gvctx) <= 0) {
		printf("sign: in-guest P-256 key FAILED\nSIGN RESULT: FAIL\n"); return 1;
	}
	printf("sign: %s, %d samples per arm after %d warm-up calls; columns: arm min p50 p99 max\n", OpenSSL_version(OPENSSL_VERSION), NSAMPLES, NWARM);
	memcpy(map + OFF_DG, dg, TLSKEY_DIGEST_LEN);
	for (int i = 0; i < NWARM; i++) { gsiglen = sizeof(gsig); EVP_PKEY_sign(gctx, gsig, &gsiglen, dg, sizeof(dg)); }
	for (int i = 0; i < NSAMPLES; i++) {
		gsiglen = sizeof(gsig);
		uint64_t t0 = t_begin();
		int ok = EVP_PKEY_sign(gctx, gsig, &gsiglen, dg, sizeof(dg));
		uint64_t t1 = t_end();
		ig[i] = t1 - t0;
		if (ok <= 0 || !verify(gvctx, gsig, gsiglen, dg)) bad_ig++;
	}
	for (int i = 0; i < NWARM; i++) sign_errand();
	for (int i = 0; i < NSAMPLES; i++) {
		uint64_t t0 = t_begin();
		r = sign_errand();
		uint64_t t1 = t_end();
		er[i] = t1 - t0;
		if (r <= 0 || r > TLSKEY_SIG_MAX) { bad_er++; continue; }
		memcpy(sig, map + OFF_SIG, (size_t)r);
		if (!verify(vctx, sig, (size_t)r, dg)) bad_er++;
	}
	pi = percentiles(ig, NSAMPLES);
	pe = percentiles(er, NSAMPLES);
	printf("SIGNBENCH in-guest %8lu %8lu %8lu %10lu\n", pi.min, pi.p50, pi.p99, pi.max);
	printf("SIGNBENCH errand   %8lu %8lu %8lu %10lu   diff p50 %+ld\n", pe.min, pe.p50, pe.p99, pe.max, (long)(pe.p50 - pi.p50));
	printf("sign: signatures that did not verify: in-guest %d, errand %d (of %d each)\n", bad_ig, bad_er, NSAMPLES);
	printf("sign: errands served by the service so far: %lu\n", errand_call(TLSKEY_SLOT, TLSKEY_OP_COUNTERS, 0, 0, 0));
	printf("SIGN RESULT: %s\n", bad_ig || bad_er ? "FAIL (bad signatures)" : "DONE");

	return bad_ig || bad_er;
}
