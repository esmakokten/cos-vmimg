/*
 * provider_check -- step 2.2's gate: the errand provider against the in-guest (default) one.
 *
 * Registers the built-in "errand" provider, fetches AES-128-GCM from it and from the default
 * provider, and drives both exactly as libssl's TLS 1.3 record layer does (init with key once;
 * per record: init with IV, [decrypt: SET_TAG], AAD update, one data update, final,
 * [encrypt: GET_TAG]). Checks NIST GCM test case 2 through the errand, then random records of
 * 16 B .. 16 KB with a 5-byte AAD: ciphertext and tag must match the default provider's, each
 * side must open the other's output, and a corrupted tag must be rejected by the errand side.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <openssl/provider.h>
#include <openssl/evp.h>
#include <openssl/err.h>
#include <openssl/rand.h>

extern int OSSL_provider_init_errand(const OSSL_CORE_HANDLE *, const OSSL_DISPATCH *, const OSSL_DISPATCH **, void **);
extern unsigned long errand_provider_records(void);

static inline uint64_t rdtsc_(void) { unsigned a, d; asm volatile("lfence\n\trdtsc" : "=a"(a), "=d"(d)); return ((uint64_t)d << 32) | a; }

static int
tls_seal(EVP_CIPHER_CTX *ctx, const unsigned char *iv, const unsigned char *aad, int aadlen,
	 const unsigned char *in, int inl, unsigned char *out, unsigned char *tag)
{
	int lenu = 0, lenf = 0;
	if (EVP_CipherInit_ex(ctx, NULL, NULL, NULL, iv, 1) <= 0) return 0;
	if (aadlen && EVP_CipherUpdate(ctx, NULL, &lenu, aad, aadlen) <= 0) return 0;
	if (EVP_CipherUpdate(ctx, out, &lenu, in, inl) <= 0) return 0;
	if (EVP_CipherFinal_ex(ctx, out + lenu, &lenf) <= 0) return 0;
	if (lenu + lenf != inl) return 0;
	return EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_GET_TAG, 16, tag) > 0;
}

static int
tls_open(EVP_CIPHER_CTX *ctx, const unsigned char *iv, const unsigned char *aad, int aadlen,
	 const unsigned char *in, int inl, const unsigned char *tag, unsigned char *out)
{
	int lenu = 0, lenf = 0;
	if (EVP_CipherInit_ex(ctx, NULL, NULL, NULL, iv, 0) <= 0) return 0;
	if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_SET_TAG, 16, (void *)tag) <= 0) return 0;
	if (aadlen && EVP_CipherUpdate(ctx, NULL, &lenu, aad, aadlen) <= 0) return 0;
	if (EVP_CipherUpdate(ctx, out, &lenu, in, inl) <= 0) return 0;
	if (EVP_CipherFinal_ex(ctx, out + lenu, &lenf) <= 0) return 0;
	return lenu + lenf == inl;
}

static EVP_CIPHER_CTX *
ctx_with_key(EVP_CIPHER *ciph, const unsigned char *key, int enc)
{
	EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
	if (EVP_CipherInit_ex(ctx, ciph, NULL, NULL, NULL, enc) <= 0
	    || EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_SET_IVLEN, 12, NULL) <= 0
	    || EVP_CipherInit_ex(ctx, NULL, NULL, key, NULL, -1) <= 0) { EVP_CIPHER_CTX_free(ctx); return NULL; }
	return ctx;
}

int
main(void)
{
	static const unsigned char tc2_c[16] = {0x03,0x88,0xda,0xce,0x60,0xb6,0xa3,0x92,0xf3,0x28,0xc2,0xb9,0x71,0xb2,0xfe,0x78};
	static const unsigned char tc2_t[16] = {0xab,0x6e,0x47,0xd4,0x2c,0xec,0x13,0xbd,0xf5,0x3a,0x67,0xb2,0x12,0x57,0xbd,0xdf};
	unsigned char zero[16] = {0}, key[16], iv[12], aad[5] = {0x17, 0x03, 0x03, 0, 0};
	unsigned char *p, *c1, *c2, *d, tag1[16], tag2[16];
	int ok = 1;

	if (OSSL_PROVIDER_add_builtin(NULL, "errand", OSSL_provider_init_errand) != 1) { printf("provider_check: add_builtin failed\n"); return 1; }
	OSSL_PROVIDER *pe = OSSL_PROVIDER_load(NULL, "errand"), *pd = OSSL_PROVIDER_load(NULL, "default");
	if (!pe || !pd) { printf("provider_check: load failed\n"); ERR_print_errors_fp(stdout); return 1; }
	EVP_CIPHER *ce = EVP_CIPHER_fetch(NULL, "AES-128-GCM", "provider=errand");
	EVP_CIPHER *cd = EVP_CIPHER_fetch(NULL, "AES-128-GCM", "provider=default");
	if (!ce || !cd) { printf("provider_check: fetch failed (errand %p default %p)\n", (void *)ce, (void *)cd); ERR_print_errors_fp(stdout); return 1; }
	printf("provider_check: AES-128-GCM from '%s' and '%s'\n", OSSL_PROVIDER_get0_name(EVP_CIPHER_get0_provider(ce)),
	       OSSL_PROVIDER_get0_name(EVP_CIPHER_get0_provider(cd)));
	/* what libssl asks before using a cipher */
	printf("provider_check: errand cipher: mode %d (gcm=%d), ivlen %d, keylen %d, aead %d\n", EVP_CIPHER_get_mode(ce), EVP_CIPH_GCM_MODE,
	       EVP_CIPHER_get_iv_length(ce), EVP_CIPHER_get_key_length(ce), (EVP_CIPHER_get_flags(ce) & EVP_CIPH_FLAG_AEAD_CIPHER) != 0);

	/* NIST test case 2 through the errand */
	{
		EVP_CIPHER_CTX *e = ctx_with_key(ce, zero, 1); unsigned char out[16], tag[16], back[16];
		if (!e || !tls_seal(e, zero, NULL, 0, zero, 16, out, tag) || memcmp(out, tc2_c, 16) || memcmp(tag, tc2_t, 16)) { printf("provider_check: NIST case 2 SEAL WRONG\n"); ok = 0; }
		else printf("provider_check: NIST case 2 SEAL ok\n");
		EVP_CIPHER_CTX *o = ctx_with_key(ce, zero, 0);
		if (!o || !tls_open(o, zero, NULL, 0, tc2_c, 16, tc2_t, back) || memcmp(back, zero, 16)) { printf("provider_check: NIST case 2 OPEN WRONG\n"); ok = 0; }
		else printf("provider_check: NIST case 2 OPEN ok\n");
		EVP_CIPHER_CTX_free(e); EVP_CIPHER_CTX_free(o);
	}

	RAND_bytes(key, 16);
	p = malloc(16384); c1 = malloc(16384); c2 = malloc(16384); d = malloc(16384);
	RAND_bytes(p, 16384);
	EVP_CIPHER_CTX *ee = ctx_with_key(ce, key, 1), *eo = ctx_with_key(ce, key, 0), *de = ctx_with_key(cd, key, 1), *dop = ctx_with_key(cd, key, 0);
	if (!ee || !eo || !de || !dop) { printf("provider_check: key init failed\n"); return 1; }
	static const int sizes[] = {16, 256, 1024, 4096, 16384};
	for (unsigned s = 0; s < 5; s++) {
		int n = sizes[s];
		RAND_bytes(iv, 12); aad[3] = (unsigned char)((n + 17) >> 8); aad[4] = (unsigned char)(n + 17);
		int r1 = tls_seal(ee, iv, aad, 5, p, n, c1, tag1), r2 = tls_seal(de, iv, aad, 5, p, n, c2, tag2);
		int same = r1 && r2 && !memcmp(c1, c2, n) && !memcmp(tag1, tag2, 16);
		int x1 = tls_open(dop, iv, aad, 5, c1, n, tag1, d) && !memcmp(d, p, n);     /* default opens the errand's */
		int x2 = tls_open(eo, iv, aad, 5, c2, n, tag2, d) && !memcmp(d, p, n);      /* the errand opens the default's */
		tag2[0] ^= 1;
		int rej = !tls_open(eo, iv, aad, 5, c2, n, tag2, d);                       /* corrupted tag rejected */
		printf("provider_check: %5d B: seal match %s, default opens errand %s, errand opens default %s, corrupted tag rejected %s\n",
		       n, same ? "ok" : "NO", x1 ? "ok" : "NO", x2 ? "ok" : "NO", rej ? "ok" : "NO");
		ok &= same && x1 && x2 && rej;
	}
	/* a dup of a keyed context must work (nginx does not need it, but EVP allows it) */
	{
		EVP_CIPHER_CTX *dup = EVP_CIPHER_CTX_new(); unsigned char out[64], tag[16];
		int r = EVP_CIPHER_CTX_copy(dup, ee) && tls_seal(dup, iv, aad, 5, p, 64, out, tag) && tls_seal(de, iv, aad, 5, p, 64, c2, tag2) && !memcmp(out, c2, 64) && !memcmp(tag, tag2, 16);
		printf("provider_check: dupctx %s\n", r ? "ok" : "NO"); ok &= r;
		EVP_CIPHER_CTX_free(dup);
	}
	/* a quick cycle count per 4 KB record, both arms, for sanity (not a measurement) */
	{
		uint64_t t0, t1; int N = 1000;
		t0 = rdtsc_(); for (int i = 0; i < N; i++) { iv[0] = (unsigned char)i; tls_seal(ee, iv, aad, 5, p, 4096, c1, tag1); } t1 = rdtsc_();
		uint64_t te = (t1 - t0) / N;
		t0 = rdtsc_(); for (int i = 0; i < N; i++) { iv[0] = (unsigned char)i; tls_seal(de, iv, aad, 5, p, 4096, c2, tag2); } t1 = rdtsc_();
		printf("provider_check: 4 KB SEAL through EVP: errand %lu cycles, default %lu cycles (mean of %d)\n", (unsigned long)te, (unsigned long)((t1 - t0) / N), N);
	}
	printf("provider_check: records through the provider in this process: %lu (mode %s)\n", errand_provider_records(), getenv("TLS_ERRAND") ? getenv("TLS_ERRAND") : "errand");
	EVP_CIPHER_CTX_free(ee); EVP_CIPHER_CTX_free(eo); EVP_CIPHER_CTX_free(de); EVP_CIPHER_CTX_free(dop);
	printf("PROVIDER CHECK: %s\n", ok ? "PASS" : "FAIL");
	return ok ? 0 : 1;
}
