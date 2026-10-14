/*
 * errand_provider.c -- an OpenSSL 3.0 built-in provider with one algorithm, AES-128-GCM,
 * served by the tlskey errand: the key goes to the service once (SETKEY), and every record
 * is one SEAL or OPEN errand through the shared buffer that errandbuf.ko maps.
 *
 * What libssl's TLS 1.3 record layer does per record (ssl/record/ssl3_record_tls13.c,
 * results/check-provider-api/tls13_enc-excerpt.txt), and what this provider sees:
 *   change_cipher_state: CipherInit(cipher, no key)      -> einit/dinit(key = NULL)
 *                        ctrl SET_IVLEN 12                -> set_ctx_params ivlen
 *                        CipherInit(key, no iv)           -> einit/dinit(key)   => SETKEY
 *   per record:          CipherInit(iv)                   -> einit/dinit(iv)
 *                        [recv] ctrl SET_TAG (tag ptr)    -> set_ctx_params tag
 *                        CipherUpdate(out = NULL, header) -> update: AAD (5 bytes)
 *                        CipherUpdate(data, whole record) -> update: SEAL or OPEN, one errand
 *                        CipherFinal                      -> final: nothing left
 *                        [send] ctrl GET_TAG              -> get_ctx_params tag
 * The provider therefore treats each data update as one complete record (the one errand), which
 * is also what the generic EVP one-shot usage does. Several AAD updates are accumulated; a
 * second data update before a new IV is refused.
 *
 * Selected from nginx by `EVP_set_default_properties(NULL, "?provider=errand")`: the '?' makes
 * it a preference, so every other algorithm (SHA-256, HKDF, X25519, ECDSA) still comes from the
 * default provider. In record mode the session key is derived by libssl inside the guest and
 * handed to the service (DECISIONS.md Q2); the ctx keeps a copy only for dupctx.
 *
 * Phase 3 of w7 (cut 1), TLS_ERRAND=sign or sign+records: the server's ECDSA P-256 key exists only
 * in the service. nginx loads a placeholder key file (the certificate's public point, scalar 1);
 * this provider adds an EC keymgmt, which imports only the public point (and refuses a point that
 * is not the service's), and an ECDSA signature, so that libssl's CertificateVerify
 * (EVP_DigestSignInit_ex + EVP_DigestSign under "?provider=errand") lands here: the transcript is
 * hashed in the guest (public data), and the 32-byte digest goes to the service in one SIGN errand,
 * which returns the DER signature. Verification is not offered (the default provider keeps it).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <openssl/core.h>
#include <openssl/core_dispatch.h>
#include <openssl/core_names.h>
#include <openssl/params.h>
#include <openssl/provider.h>
#include <openssl/evp.h>
#include <openssl/err.h>
#include "libErrand.h"
#include "tlskey_abi.h"

/*
 * Three arms share this one provider. TLS_ERRAND unset: the provider is not registered and
 * nginx uses OpenSSL's own AES-GCM (the stock in-guest arm). TLS_ERRAND=1: every record is an
 * errand. TLS_ERRAND=local: the same code path -- same staging buffer, same copies, same cipher
 * calls as the service's (OpenSSL 3.0.17's EVP AES-GCM, the default provider's implementation
 * that the stock arm runs too) -- but in this process instead of in the service, so
 * errand - local is the crossing alone and local - stock is the two copies per record.
 */
static int         local_mode;
static int         cipher_mode; /* AES-128-GCM offered: TLS_ERRAND=1, local, sign+records */
static int         sign_mode;   /* EC keymgmt + ECDSA offered: TLS_ERRAND=sign, sign+records */
static int         sign_check;  /* diagnostic, TLS_ERRAND_CHECK=1: verify every SIGN in the guest (never in a measured arm) */
static EVP_CIPHER *local_cipher; /* local mode: "provider=default", never this provider itself */

/* The shared buffer, mapped once at provider init. Layout used by this provider:
 *   [0, 64)          key scratch for SETKEY (zeroed after the errand)
 *   [1024, 1056)     SIGN: the digest
 *   [1088, 1160)     SIGN: the DER signature; PUBKEY: the public point
 *   [4096, 4096+72K) one record: [nonce 12][aad_len 2][aad][payload (+ tag)]            */
#define EB_KEY_OFF     0UL
#define EB_SIGN_IN     1024UL
#define EB_SIGN_OUT    1088UL
#define EB_REC_OFF     4096UL
#define EB_MAX_PAYLOAD (64UL * 1024)

static unsigned char *ebuf;
static unsigned long  ebuf_len;
static unsigned long  errand_records; /* SEAL + OPEN errands issued by this process */
static unsigned long  errand_signs;   /* SIGN errands issued by this process */

static int
ebuf_map(void)
{
	long len;
	int  fd;

	if (ebuf) return 1;
	if (local_mode) { /* a private staging buffer of the same layout */
		ebuf_len = 2UL * 1024 * 1024;
		ebuf = calloc(1, ebuf_len);
		return ebuf != NULL;
	}
	len = (long)errand_call(TLSKEY_SLOT, TLSKEY_OP_INFO_LEN, 0, 0, 0);
	if (len <= 0) return 0;
	fd = open("/dev/errandbuf", O_RDWR);
	if (fd < 0) return 0;
	ebuf = mmap(NULL, (size_t)len, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	close(fd);
	if (ebuf == MAP_FAILED) { ebuf = NULL; return 0; }
	ebuf_len = (unsigned long)len;

	return EB_REC_OFF + TLSKEY_HDR_LEN + TLSKEY_MAX_AAD + EB_MAX_PAYLOAD + TLSKEY_TAG_LEN <= ebuf_len;
}

struct ectx {
	int           enc;
	unsigned long kid;     /* service key id, 0 = none (local mode: 1 once keyed) */
	EVP_CIPHER_CTX *lctx;  /* local mode: the service's per-slot ctx, in-process */
	unsigned char key[16];
	size_t        keylen;
	unsigned char iv[TLSKEY_NONCE_LEN];
	size_t        ivlen;
	int           iv_set;
	unsigned char aad[TLSKEY_MAX_AAD];
	size_t        aadlen;
	unsigned char tag[TLSKEY_TAG_LEN];
	size_t        taglen;
	int           tag_set;   /* a tag was given for OPEN */
	int           tag_ready; /* a tag was produced by SEAL */
	int           done;      /* the record for this IV has been processed */
};

static int
key_install(struct ectx *c, const unsigned char *key, size_t keylen)
{
	long kid;

	if (keylen != 16 || !ebuf) return 0;
	if (local_mode) {
		/* fetched here, not at provider init: nginx's "?provider=errand" preference must not
		 * resolve this ctx to the errand provider itself */
		if (!local_cipher && !(local_cipher = EVP_CIPHER_fetch(NULL, "AES-128-GCM", "provider=default"))) return 0;
		if (!c->lctx && !(c->lctx = EVP_CIPHER_CTX_new())) return 0;
		if (!EVP_CipherInit_ex(c->lctx, local_cipher, NULL, key, NULL, 1)) return 0;
		c->kid = 1;
		memcpy(c->key, key, 16);
		c->keylen = 16;
		return 1;
	}
	memcpy(ebuf + EB_KEY_OFF, key, 16);
	kid = (long)errand_call(TLSKEY_SLOT, TLSKEY_OP_SETKEY, EB_KEY_OFF, 16, 0);
	memset(ebuf + EB_KEY_OFF, 0, 16);
	if (kid <= 0) return 0;
	if (c->kid) errand_call(TLSKEY_SLOT, TLSKEY_OPKEY(TLSKEY_OP_DELKEY, c->kid), 0, 0, 0);
	c->kid = (unsigned long)kid;
	memcpy(c->key, key, 16);
	c->keylen = 16;

	return 1;
}

static void *
ep_newctx(void *provctx)
{
	struct ectx *c = calloc(1, sizeof(*c));

	if (!c) return NULL;
	c->ivlen  = TLSKEY_NONCE_LEN;
	c->taglen = TLSKEY_TAG_LEN;
	c->keylen = 16;

	return c;
}

static void
ep_freectx(void *vctx)
{
	struct ectx *c = vctx;

	if (!c) return;
	if (c->kid && !local_mode) errand_call(TLSKEY_SLOT, TLSKEY_OPKEY(TLSKEY_OP_DELKEY, c->kid), 0, 0, 0);
	EVP_CIPHER_CTX_free(c->lctx);
	memset(c, 0, sizeof(*c));
	free(c);
}

static void *
ep_dupctx(void *vctx)
{
	struct ectx *src = vctx, *c;

	c = malloc(sizeof(*c));
	if (!c) return NULL;
	*c = *src;
	c->kid  = 0;
	c->lctx = NULL;
	if (src->kid && !key_install(c, src->key, src->keylen)) { free(c); return NULL; }

	return c;
}

static int ep_set_ctx_params(void *vctx, const OSSL_PARAM params[]);

static int
ep_init(void *vctx, const unsigned char *key, size_t keylen, const unsigned char *iv, size_t ivlen,
	const OSSL_PARAM params[], int enc)
{
	struct ectx *c = vctx;

	c->enc = enc;
	if (key && !key_install(c, key, keylen)) return 0;
	if (iv) {
		if (ivlen != TLSKEY_NONCE_LEN) return 0;
		memcpy(c->iv, iv, TLSKEY_NONCE_LEN);
		c->iv_set = 1;
	}
	/* a new init starts a new record */
	c->aadlen = 0;
	c->tag_set = 0;
	c->tag_ready = 0;
	c->done = 0;

	return ep_set_ctx_params(c, params);
}

static int ep_einit(void *c, const unsigned char *k, size_t kl, const unsigned char *iv, size_t il, const OSSL_PARAM p[]) { return ep_init(c, k, kl, iv, il, p, 1); }
static int ep_dinit(void *c, const unsigned char *k, size_t kl, const unsigned char *iv, size_t il, const OSSL_PARAM p[]) { return ep_init(c, k, kl, iv, il, p, 0); }

/*
 * Local mode: the service's gcm_record (tlskey.c), the same EVP calls in the same order:
 * CipherInit(nonce, direction), [OPEN] SET_TAG, one AAD update, one payload update in place,
 * CipherFinal, [SEAL] GET_TAG. Returns 1, or 0 (for OPEN: the tag did not verify).
 */
static int
local_record(EVP_CIPHER_CTX *ctx, int enc, const unsigned char *nonce, const unsigned char *aad,
	     size_t aad_len, unsigned char *m, size_t len, unsigned char *tag)
{
	unsigned char fin[16];
	int outl;

	if (len > 0x7fffffff || !EVP_CipherInit_ex(ctx, NULL, NULL, NULL, nonce, enc)) return 0;
	if (!enc && !EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_SET_TAG, TLSKEY_TAG_LEN, tag)) return 0;
	if (aad_len && !EVP_CipherUpdate(ctx, NULL, &outl, aad, (int)aad_len)) return 0;
	if (len && (!EVP_CipherUpdate(ctx, m, &outl, m, (int)len) || (size_t)outl != len)) return 0;
	if (!EVP_CipherFinal_ex(ctx, fin, &outl)) { ERR_clear_error(); return 0; }
	if (enc && !EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_GET_TAG, TLSKEY_TAG_LEN, tag)) return 0;

	return 1;
}

/* One record through the buffer: header, payload, one errand, output back. */
static int
ep_record(struct ectx *c, unsigned char *out, size_t *outl, size_t outsize, const unsigned char *in, size_t inl)
{
	unsigned char *rec = ebuf + EB_REC_OFF;
	unsigned long  payload_off = EB_REC_OFF + TLSKEY_HDR_LEN + c->aadlen;
	unsigned char *payload = ebuf + payload_off;
	long r;

	if (!c->kid || !c->iv_set || c->done || inl > EB_MAX_PAYLOAD || outsize < inl) return 0;
	memcpy(rec, c->iv, TLSKEY_NONCE_LEN);
	rec[12] = (unsigned char)c->aadlen;
	rec[13] = (unsigned char)(c->aadlen >> 8);
	memcpy(rec + TLSKEY_HDR_LEN, c->aad, c->aadlen);
	memcpy(payload, in, inl);
	errand_records++;
	if (local_mode) { /* the service's op_seal/op_open, in-process */
		if (!c->enc && !c->tag_set) return 0;
		if (!local_record(c->lctx, c->enc, c->iv, c->aad, c->aadlen, payload, inl, c->tag)) return 0;
		if (c->enc) c->tag_ready = 1;
		memcpy(out, payload, inl);
		c->done = 1;
		*outl = inl;
		return 1;
	}
	if (c->enc) {
		r = (long)errand_call(TLSKEY_SLOT, TLSKEY_OPKEY(TLSKEY_OP_SEAL, c->kid), EB_REC_OFF,
				      TLSKEY_HDR_LEN + c->aadlen + inl, payload_off);
		if (r) return 0;
		memcpy(out, payload, inl);
		memcpy(c->tag, payload + inl, TLSKEY_TAG_LEN);
		c->tag_ready = 1;
	} else {
		if (!c->tag_set || c->taglen != TLSKEY_TAG_LEN) return 0; /* TLS sets the tag before the data */
		memcpy(payload + inl, c->tag, TLSKEY_TAG_LEN);
		r = (long)errand_call(TLSKEY_SLOT, TLSKEY_OPKEY(TLSKEY_OP_OPEN, c->kid), EB_REC_OFF,
				      TLSKEY_HDR_LEN + c->aadlen + inl + TLSKEY_TAG_LEN, payload_off);
		if (r) return 0; /* TLSKEY_EBADMSG: bad record MAC */
		memcpy(out, payload, inl);
	}
	c->done = 1;
	*outl = inl;

	return 1;
}

static int
ep_update(void *vctx, unsigned char *out, size_t *outl, size_t outsize, const unsigned char *in, size_t inl)
{
	struct ectx *c = vctx;

	if (out == NULL) { /* AAD */
		if (inl > TLSKEY_MAX_AAD - c->aadlen) return 0;
		memcpy(c->aad + c->aadlen, in, inl);
		c->aadlen += inl;
		*outl = inl;
		return 1;
	}
	if (inl == 0) { *outl = 0; return 1; }

	return ep_record(c, out, outl, outsize, in, inl);
}

static int
ep_final(void *vctx, unsigned char *out, size_t *outl, size_t outsize)
{
	*outl = 0;

	return 1;
}

static int
ep_cipher(void *vctx, unsigned char *out, size_t *outl, size_t outsize, const unsigned char *in, size_t inl)
{
	return ep_update(vctx, out, outl, outsize, in, inl);
}

static int
ep_get_params(OSSL_PARAM params[])
{
	OSSL_PARAM *p;

	if ((p = OSSL_PARAM_locate(params, OSSL_CIPHER_PARAM_MODE)) && !OSSL_PARAM_set_uint(p, EVP_CIPH_GCM_MODE)) return 0;
	if ((p = OSSL_PARAM_locate(params, OSSL_CIPHER_PARAM_AEAD)) && !OSSL_PARAM_set_int(p, 1)) return 0;
	if ((p = OSSL_PARAM_locate(params, OSSL_CIPHER_PARAM_CUSTOM_IV)) && !OSSL_PARAM_set_int(p, 1)) return 0;
	if ((p = OSSL_PARAM_locate(params, OSSL_CIPHER_PARAM_CTS)) && !OSSL_PARAM_set_int(p, 0)) return 0;
	if ((p = OSSL_PARAM_locate(params, OSSL_CIPHER_PARAM_TLS1_MULTIBLOCK)) && !OSSL_PARAM_set_int(p, 0)) return 0;
	if ((p = OSSL_PARAM_locate(params, OSSL_CIPHER_PARAM_HAS_RAND_KEY)) && !OSSL_PARAM_set_int(p, 0)) return 0;
	if ((p = OSSL_PARAM_locate(params, OSSL_CIPHER_PARAM_KEYLEN)) && !OSSL_PARAM_set_size_t(p, 16)) return 0;
	if ((p = OSSL_PARAM_locate(params, OSSL_CIPHER_PARAM_IVLEN)) && !OSSL_PARAM_set_size_t(p, TLSKEY_NONCE_LEN)) return 0;
	if ((p = OSSL_PARAM_locate(params, OSSL_CIPHER_PARAM_BLOCK_SIZE)) && !OSSL_PARAM_set_size_t(p, 1)) return 0;

	return 1;
}

static const OSSL_PARAM ep_gettable_params[] = {
	OSSL_PARAM_uint(OSSL_CIPHER_PARAM_MODE, NULL),
	OSSL_PARAM_int(OSSL_CIPHER_PARAM_AEAD, NULL),
	OSSL_PARAM_int(OSSL_CIPHER_PARAM_CUSTOM_IV, NULL),
	OSSL_PARAM_int(OSSL_CIPHER_PARAM_CTS, NULL),
	OSSL_PARAM_int(OSSL_CIPHER_PARAM_TLS1_MULTIBLOCK, NULL),
	OSSL_PARAM_int(OSSL_CIPHER_PARAM_HAS_RAND_KEY, NULL),
	OSSL_PARAM_size_t(OSSL_CIPHER_PARAM_KEYLEN, NULL),
	OSSL_PARAM_size_t(OSSL_CIPHER_PARAM_IVLEN, NULL),
	OSSL_PARAM_size_t(OSSL_CIPHER_PARAM_BLOCK_SIZE, NULL),
	OSSL_PARAM_END
};
static const OSSL_PARAM *ep_gettable(void *provctx) { return ep_gettable_params; }

static int
ep_get_ctx_params(void *vctx, OSSL_PARAM params[])
{
	struct ectx *c = vctx;
	OSSL_PARAM *p;

	if ((p = OSSL_PARAM_locate(params, OSSL_CIPHER_PARAM_IVLEN)) && !OSSL_PARAM_set_size_t(p, c->ivlen)) return 0;
	if ((p = OSSL_PARAM_locate(params, OSSL_CIPHER_PARAM_KEYLEN)) && !OSSL_PARAM_set_size_t(p, c->keylen)) return 0;
	if ((p = OSSL_PARAM_locate(params, OSSL_CIPHER_PARAM_AEAD_TAGLEN)) && !OSSL_PARAM_set_size_t(p, c->taglen)) return 0;
	if ((p = OSSL_PARAM_locate(params, OSSL_CIPHER_PARAM_IV))
	    && !OSSL_PARAM_set_octet_string(p, c->iv, c->ivlen) && !OSSL_PARAM_set_octet_ptr(p, c->iv, c->ivlen)) return 0;
	if ((p = OSSL_PARAM_locate(params, OSSL_CIPHER_PARAM_UPDATED_IV))
	    && !OSSL_PARAM_set_octet_string(p, c->iv, c->ivlen) && !OSSL_PARAM_set_octet_ptr(p, c->iv, c->ivlen)) return 0;
	if ((p = OSSL_PARAM_locate(params, OSSL_CIPHER_PARAM_AEAD_TAG))) {
		if (!c->enc || !c->tag_ready || p->data_size != TLSKEY_TAG_LEN) return 0;
		if (!OSSL_PARAM_set_octet_string(p, c->tag, TLSKEY_TAG_LEN)) return 0;
	}

	return 1;
}

static const OSSL_PARAM ep_gettable_ctx_params[] = {
	OSSL_PARAM_size_t(OSSL_CIPHER_PARAM_KEYLEN, NULL),
	OSSL_PARAM_size_t(OSSL_CIPHER_PARAM_IVLEN, NULL),
	OSSL_PARAM_size_t(OSSL_CIPHER_PARAM_AEAD_TAGLEN, NULL),
	OSSL_PARAM_octet_string(OSSL_CIPHER_PARAM_IV, NULL, 0),
	OSSL_PARAM_octet_string(OSSL_CIPHER_PARAM_UPDATED_IV, NULL, 0),
	OSSL_PARAM_octet_string(OSSL_CIPHER_PARAM_AEAD_TAG, NULL, 0),
	OSSL_PARAM_END
};
static const OSSL_PARAM *ep_gettable_ctx(void *cctx, void *provctx) { return ep_gettable_ctx_params; }

static int
ep_set_ctx_params(void *vctx, const OSSL_PARAM params[])
{
	struct ectx *c = vctx;
	const OSSL_PARAM *p;
	size_t sz;

	if (params == NULL) return 1;
	if ((p = OSSL_PARAM_locate_const(params, OSSL_CIPHER_PARAM_AEAD_TAG))) {
		if (p->data_type != OSSL_PARAM_OCTET_STRING || p->data_size != TLSKEY_TAG_LEN) return 0;
		c->taglen = TLSKEY_TAG_LEN;
		if (p->data) { /* NULL data only sets the length (change_cipher_state does that for CCM) */
			memcpy(c->tag, p->data, TLSKEY_TAG_LEN);
			c->tag_set = 1;
		}
	}
	if ((p = OSSL_PARAM_locate_const(params, OSSL_CIPHER_PARAM_AEAD_IVLEN))) {
		if (!OSSL_PARAM_get_size_t(p, &sz) || sz != TLSKEY_NONCE_LEN) return 0;
		c->ivlen = sz;
	}

	return 1;
}

static const OSSL_PARAM ep_settable_ctx_params[] = {
	OSSL_PARAM_octet_string(OSSL_CIPHER_PARAM_AEAD_TAG, NULL, 0),
	OSSL_PARAM_size_t(OSSL_CIPHER_PARAM_AEAD_IVLEN, NULL),
	OSSL_PARAM_END
};
static const OSSL_PARAM *ep_settable_ctx(void *cctx, void *provctx) { return ep_settable_ctx_params; }

static const OSSL_DISPATCH ep_aes128gcm_functions[] = {
	{ OSSL_FUNC_CIPHER_NEWCTX, (void (*)(void))ep_newctx },
	{ OSSL_FUNC_CIPHER_FREECTX, (void (*)(void))ep_freectx },
	{ OSSL_FUNC_CIPHER_DUPCTX, (void (*)(void))ep_dupctx },
	{ OSSL_FUNC_CIPHER_ENCRYPT_INIT, (void (*)(void))ep_einit },
	{ OSSL_FUNC_CIPHER_DECRYPT_INIT, (void (*)(void))ep_dinit },
	{ OSSL_FUNC_CIPHER_UPDATE, (void (*)(void))ep_update },
	{ OSSL_FUNC_CIPHER_FINAL, (void (*)(void))ep_final },
	{ OSSL_FUNC_CIPHER_CIPHER, (void (*)(void))ep_cipher },
	{ OSSL_FUNC_CIPHER_GET_PARAMS, (void (*)(void))ep_get_params },
	{ OSSL_FUNC_CIPHER_GETTABLE_PARAMS, (void (*)(void))ep_gettable },
	{ OSSL_FUNC_CIPHER_GET_CTX_PARAMS, (void (*)(void))ep_get_ctx_params },
	{ OSSL_FUNC_CIPHER_GETTABLE_CTX_PARAMS, (void (*)(void))ep_gettable_ctx },
	{ OSSL_FUNC_CIPHER_SET_CTX_PARAMS, (void (*)(void))ep_set_ctx_params },
	{ OSSL_FUNC_CIPHER_SETTABLE_CTX_PARAMS, (void (*)(void))ep_settable_ctx },
	{ 0, NULL }
};

static const OSSL_ALGORITHM ep_ciphers[] = {
	{ "AES-128-GCM:id-aes128-GCM:2.16.840.1.101.3.4.1.6", "provider=errand", ep_aes128gcm_functions,
	  "AES-128-GCM through the Composite tlskey errand" },
	{ NULL, NULL, NULL, NULL }
};

/* --- Phase 3: the EC keymgmt (public point only) and the ECDSA signature (one SIGN errand) --- */

static unsigned char svc_pub[TLSKEY_PUB_LEN]; /* the service's public point, read once (PUBKEY) */
static int           svc_pub_ok;

static int
svc_pubkey(void)
{
	if (svc_pub_ok) return 1;
	if ((long)errand_call(TLSKEY_SLOT, TLSKEY_OP_PUBKEY, EB_SIGN_OUT, 0, 0) != TLSKEY_PUB_LEN) return 0;
	memcpy(svc_pub, ebuf + EB_SIGN_OUT, TLSKEY_PUB_LEN);
	svc_pub_ok = 1;

	return 1;
}

struct ekey {
	int           has_pub;
	unsigned char pub[TLSKEY_PUB_LEN];
};

static void *ek_new(void *provctx) { return calloc(1, sizeof(struct ekey)); }
static void  ek_free(void *k) { free(k); }

static int
ek_has(const void *vk, int selection)
{
	const struct ekey *k = vk;

	/* the private half is the service's: present whenever the public point is (ek_import checked it) */
	if (!k) return 0;
	if (selection & OSSL_KEYMGMT_SELECT_KEYPAIR) return k->has_pub;

	return 1; /* the domain parameters are P-256, always */
}

/*
 * Only the public point is taken, and only if it is the service's: a placeholder for any other key
 * would sign with a key whose certificate it is not. The group must be P-256. Any private scalar
 * in the params (the placeholder's 1) is ignored.
 */
static int
ek_import(void *vk, int selection, const OSSL_PARAM params[])
{
	struct ekey *k = vk;
	const OSSL_PARAM *p;
	const char *g = NULL;
	const void *pub = NULL;
	size_t publen = 0;

	if (!k || !(selection & OSSL_KEYMGMT_SELECT_PUBLIC_KEY)) return 0;
	if (!(p = OSSL_PARAM_locate_const(params, OSSL_PKEY_PARAM_GROUP_NAME)) || !OSSL_PARAM_get_utf8_string_ptr(p, &g)
	    || (strcasecmp(g, "prime256v1") && strcasecmp(g, "P-256")))
		return 0;
	if (!(p = OSSL_PARAM_locate_const(params, OSSL_PKEY_PARAM_PUB_KEY)) || !OSSL_PARAM_get_octet_string_ptr(p, &pub, &publen)
	    || publen != TLSKEY_PUB_LEN)
		return 0;
	if (!svc_pubkey() || memcmp(pub, svc_pub, TLSKEY_PUB_LEN)) {
		fprintf(stderr, "errand provider: the key's public point is not the tlskey service's\n");
		return 0;
	}
	memcpy(k->pub, pub, TLSKEY_PUB_LEN);
	k->has_pub = 1;

	return 1;
}

static const OSSL_PARAM ek_import_types_tbl[] = {
	OSSL_PARAM_utf8_string(OSSL_PKEY_PARAM_GROUP_NAME, NULL, 0),
	OSSL_PARAM_octet_string(OSSL_PKEY_PARAM_PUB_KEY, NULL, 0),
	OSSL_PARAM_BN(OSSL_PKEY_PARAM_PRIV_KEY, NULL, 0),
	OSSL_PARAM_END
};
static const OSSL_PARAM *ek_import_types(int selection) { return ek_import_types_tbl; }

static int
ek_get_params(void *vk, OSSL_PARAM params[])
{
	OSSL_PARAM *p;

	if ((p = OSSL_PARAM_locate(params, OSSL_PKEY_PARAM_BITS)) && !OSSL_PARAM_set_int(p, 256)) return 0;
	if ((p = OSSL_PARAM_locate(params, OSSL_PKEY_PARAM_SECURITY_BITS)) && !OSSL_PARAM_set_int(p, 128)) return 0;
	if ((p = OSSL_PARAM_locate(params, OSSL_PKEY_PARAM_MAX_SIZE)) && !OSSL_PARAM_set_int(p, TLSKEY_SIG_MAX)) return 0;
	if ((p = OSSL_PARAM_locate(params, OSSL_PKEY_PARAM_DEFAULT_DIGEST)) && !OSSL_PARAM_set_utf8_string(p, "SHA256")) return 0;
	if ((p = OSSL_PARAM_locate(params, OSSL_PKEY_PARAM_GROUP_NAME)) && !OSSL_PARAM_set_utf8_string(p, "prime256v1")) return 0;

	return 1;
}

static const OSSL_PARAM ek_gettable_tbl[] = {
	OSSL_PARAM_int(OSSL_PKEY_PARAM_BITS, NULL),
	OSSL_PARAM_int(OSSL_PKEY_PARAM_SECURITY_BITS, NULL),
	OSSL_PARAM_int(OSSL_PKEY_PARAM_MAX_SIZE, NULL),
	OSSL_PARAM_utf8_string(OSSL_PKEY_PARAM_DEFAULT_DIGEST, NULL, 0),
	OSSL_PARAM_utf8_string(OSSL_PKEY_PARAM_GROUP_NAME, NULL, 0),
	OSSL_PARAM_END
};
static const OSSL_PARAM *ek_gettable(void *provctx) { return ek_gettable_tbl; }

/* the signature algorithm EVP looks up for this key type (the default EC keymgmt says the same) */
static const char *ek_query_op(int operation_id) { return operation_id == OSSL_OP_SIGNATURE ? "ECDSA" : NULL; }

static const OSSL_DISPATCH ep_ec_keymgmt_functions[] = {
	{ OSSL_FUNC_KEYMGMT_NEW, (void (*)(void))ek_new },
	{ OSSL_FUNC_KEYMGMT_FREE, (void (*)(void))ek_free },
	{ OSSL_FUNC_KEYMGMT_HAS, (void (*)(void))ek_has },
	{ OSSL_FUNC_KEYMGMT_IMPORT, (void (*)(void))ek_import },
	{ OSSL_FUNC_KEYMGMT_IMPORT_TYPES, (void (*)(void))ek_import_types },
	{ OSSL_FUNC_KEYMGMT_GET_PARAMS, (void (*)(void))ek_get_params },
	{ OSSL_FUNC_KEYMGMT_GETTABLE_PARAMS, (void (*)(void))ek_gettable },
	{ OSSL_FUNC_KEYMGMT_QUERY_OPERATION_NAME, (void (*)(void))ek_query_op },
	{ 0, NULL }
};

static const OSSL_ALGORITHM ep_keymgmts[] = {
	{ "EC:id-ecPublicKey:1.2.840.10045.2.1", "provider=errand", ep_ec_keymgmt_functions,
	  "P-256 public point; the private key is the tlskey service's" },
	{ NULL, NULL, NULL, NULL }
};

/* The ECDSA signature: SHA-256 in the guest (the "provider=default" digest, never this
 * provider), then one SIGN errand over the 32-byte digest. */
struct esig {
	struct ekey *key;
	EVP_MD_CTX  *mdctx; /* digest-sign only */
};
static EVP_MD *sign_md;

static void *es_new(void *provctx, const char *propq) { return calloc(1, sizeof(struct esig)); }

static void
es_free(void *vs)
{
	struct esig *s = vs;

	if (!s) return;
	EVP_MD_CTX_free(s->mdctx);
	free(s);
}

/* EVP_DigestSignFinal signs on a duplicate (so that the ctx can go on hashing) */
static void *
es_dup(void *vs)
{
	struct esig *src = vs, *s = calloc(1, sizeof(*s));

	if (!s) return NULL;
	s->key = src->key;
	if (src->mdctx && (!(s->mdctx = EVP_MD_CTX_new()) || !EVP_MD_CTX_copy_ex(s->mdctx, src->mdctx))) {
		es_free(s);
		return NULL;
	}

	return s;
}

static int
es_sign_init(void *vs, void *vk, const OSSL_PARAM params[])
{
	struct esig *s = vs;
	struct ekey *k = vk;

	if (!k || !k->has_pub) return 0;
	s->key = k;

	return 1;
}

/*
 * Diagnostic (TLS_ERRAND_CHECK=1): does a SIGN's signature verify, in the guest, under the service's
 * public point and over the digest this provider sent? Prints the failing digest and signature,
 * then signs the same digest once more and says whether that one verifies. 1 = verifies.
 */
static unsigned long sign_checked, sign_bad;
static int
sign_verify(const unsigned char *dg, const unsigned char *sig, size_t siglen)
{
	static EVP_PKEY_CTX *vc;
	int r;

	if (!vc) {
		OSSL_PARAM p[] = { OSSL_PARAM_utf8_string(OSSL_PKEY_PARAM_GROUP_NAME, "prime256v1", 0),
				   OSSL_PARAM_octet_string(OSSL_PKEY_PARAM_PUB_KEY, svc_pub, TLSKEY_PUB_LEN), OSSL_PARAM_END };
		EVP_PKEY_CTX *fc = EVP_PKEY_CTX_new_from_name(NULL, "EC", "provider=default");
		EVP_PKEY *pk = NULL;

		if (!fc || EVP_PKEY_fromdata_init(fc) <= 0 || EVP_PKEY_fromdata(fc, &pk, EVP_PKEY_PUBLIC_KEY, p) <= 0) return -1;
		EVP_PKEY_CTX_free(fc);
		if (!(vc = EVP_PKEY_CTX_new_from_pkey(NULL, pk, "provider=default")) || EVP_PKEY_verify_init(vc) <= 0) return -1;
	}
	r = EVP_PKEY_verify(vc, sig, siglen, dg, TLSKEY_DIGEST_LEN);
	ERR_clear_error();

	return r == 1;
}

static void
hexdump(const char *tag, const unsigned char *p, size_t n)
{
	fprintf(stderr, "%s", tag);
	for (size_t i = 0; i < n; i++) fprintf(stderr, "%02x", p[i]);
	fprintf(stderr, "\n");
}

/* One SIGN errand over a 32-byte digest; the DER signature into sig. */
static int
sign_errand(const unsigned char *dg, unsigned char *sig, size_t *siglen, size_t sigsize)
{
	long r;

	memcpy(ebuf + EB_SIGN_IN, dg, TLSKEY_DIGEST_LEN);
	r = (long)errand_call(TLSKEY_SLOT, TLSKEY_OP_SIGN, EB_SIGN_IN, TLSKEY_DIGEST_LEN, EB_SIGN_OUT);
	errand_signs++;
	if (r <= 0 || r > TLSKEY_SIG_MAX || (size_t)r > sigsize) return 0;
	memcpy(sig, ebuf + EB_SIGN_OUT, (size_t)r);
	*siglen = (size_t)r;
	if (sign_check) {
		sign_checked++;
		if (sign_verify(dg, sig, (size_t)r) != 1) {
			long r2;

			sign_bad++;
			fprintf(stderr, "errand provider: CHECK: SIGN %lu of %lu does not verify (%lu bad so far)\n", sign_checked, errand_signs, sign_bad);
			hexdump("errand provider: CHECK: digest ", dg, TLSKEY_DIGEST_LEN);
			hexdump("errand provider: CHECK: in-buffer digest after ", ebuf + EB_SIGN_IN, TLSKEY_DIGEST_LEN);
			hexdump("errand provider: CHECK: signature ", sig, (size_t)r);
			memcpy(ebuf + EB_SIGN_IN, dg, TLSKEY_DIGEST_LEN);
			r2 = (long)errand_call(TLSKEY_SLOT, TLSKEY_OP_SIGN, EB_SIGN_IN, TLSKEY_DIGEST_LEN, EB_SIGN_OUT);
			fprintf(stderr, "errand provider: CHECK: the same digest signed again: %s\n",
				r2 > 0 && r2 <= TLSKEY_SIG_MAX && sign_verify(dg, ebuf + EB_SIGN_OUT, (size_t)r2) == 1 ? "verifies" : "does NOT verify");
		} else if (sign_checked % 1000 == 0) {
			fprintf(stderr, "errand provider: CHECK: %lu SIGNs checked, %lu bad\n", sign_checked, sign_bad);
		}
	}

	return 1;
}

static int
es_sign(void *vs, unsigned char *sig, size_t *siglen, size_t sigsize, const unsigned char *tbs, size_t tbslen)
{
	struct esig *s = vs;

	if (!sig) { *siglen = TLSKEY_SIG_MAX; return 1; }
	if (!s->key || tbslen != TLSKEY_DIGEST_LEN) return 0;

	return sign_errand(tbs, sig, siglen, sigsize);
}

static int
es_digest_sign_init(void *vs, const char *mdname, void *vk, const OSSL_PARAM params[])
{
	struct esig *s = vs;

	if (mdname && strcasecmp(mdname, "SHA256") && strcasecmp(mdname, "SHA2-256")) return 0; /* ecdsa_secp256r1_sha256 only */
	if (!es_sign_init(vs, vk, params)) return 0;
	if (!sign_md && !(sign_md = EVP_MD_fetch(NULL, "SHA2-256", "provider=default"))) return 0;
	if (!s->mdctx && !(s->mdctx = EVP_MD_CTX_new())) return 0;

	return EVP_DigestInit_ex(s->mdctx, sign_md, NULL);
}

static int
es_digest_sign_update(void *vs, const unsigned char *data, size_t len)
{
	struct esig *s = vs;

	return s->mdctx && EVP_DigestUpdate(s->mdctx, data, len);
}

static int
es_digest_sign_final(void *vs, unsigned char *sig, size_t *siglen, size_t sigsize)
{
	struct esig *s = vs;
	unsigned char dg[EVP_MAX_MD_SIZE];
	unsigned int dglen = 0;

	if (!sig) { *siglen = TLSKEY_SIG_MAX; return 1; }
	if (!s->key || !s->mdctx || !EVP_DigestFinal_ex(s->mdctx, dg, &dglen) || dglen != TLSKEY_DIGEST_LEN) return 0;

	return sign_errand(dg, sig, siglen, sigsize);
}

static const OSSL_DISPATCH ep_ecdsa_functions[] = {
	{ OSSL_FUNC_SIGNATURE_NEWCTX, (void (*)(void))es_new },
	{ OSSL_FUNC_SIGNATURE_FREECTX, (void (*)(void))es_free },
	{ OSSL_FUNC_SIGNATURE_DUPCTX, (void (*)(void))es_dup },
	{ OSSL_FUNC_SIGNATURE_SIGN_INIT, (void (*)(void))es_sign_init },
	{ OSSL_FUNC_SIGNATURE_SIGN, (void (*)(void))es_sign },
	{ OSSL_FUNC_SIGNATURE_DIGEST_SIGN_INIT, (void (*)(void))es_digest_sign_init },
	{ OSSL_FUNC_SIGNATURE_DIGEST_SIGN_UPDATE, (void (*)(void))es_digest_sign_update },
	{ OSSL_FUNC_SIGNATURE_DIGEST_SIGN_FINAL, (void (*)(void))es_digest_sign_final },
	{ 0, NULL }
};

static const OSSL_ALGORITHM ep_signatures[] = {
	{ "ECDSA", "provider=errand", ep_ecdsa_functions, "ECDSA P-256 through the Composite tlskey errand (SIGN)" },
	{ NULL, NULL, NULL, NULL }
};

static const OSSL_ALGORITHM *
ep_query(void *provctx, int operation_id, int *no_cache)
{
	*no_cache = 0;

	switch (operation_id) {
	case OSSL_OP_CIPHER:    return cipher_mode ? ep_ciphers : NULL;
	case OSSL_OP_KEYMGMT:   return sign_mode ? ep_keymgmts : NULL;
	case OSSL_OP_SIGNATURE: return sign_mode ? ep_signatures : NULL;
	default:                return NULL;
	}
}

static int
ep_prov_get_params(void *provctx, OSSL_PARAM params[])
{
	OSSL_PARAM *p;

	if ((p = OSSL_PARAM_locate(params, OSSL_PROV_PARAM_NAME)) && !OSSL_PARAM_set_utf8_ptr(p, "Composite tlskey errand provider")) return 0;
	if ((p = OSSL_PARAM_locate(params, OSSL_PROV_PARAM_VERSION)) && !OSSL_PARAM_set_utf8_ptr(p, "0.1")) return 0;
	if ((p = OSSL_PARAM_locate(params, OSSL_PROV_PARAM_BUILDINFO)) && !OSSL_PARAM_set_utf8_ptr(p, "w5_tls_errand")) return 0;
	if ((p = OSSL_PARAM_locate(params, OSSL_PROV_PARAM_STATUS)) && !OSSL_PARAM_set_int(p, ebuf != NULL)) return 0;

	return 1;
}

static const OSSL_PARAM ep_prov_gettable[] = {
	OSSL_PARAM_utf8_ptr(OSSL_PROV_PARAM_NAME, NULL, 0),
	OSSL_PARAM_utf8_ptr(OSSL_PROV_PARAM_VERSION, NULL, 0),
	OSSL_PARAM_utf8_ptr(OSSL_PROV_PARAM_BUILDINFO, NULL, 0),
	OSSL_PARAM_int(OSSL_PROV_PARAM_STATUS, NULL),
	OSSL_PARAM_END
};
static const OSSL_PARAM *ep_prov_gettable_params(void *provctx) { return ep_prov_gettable; }

static void ep_teardown(void *provctx) { }

static const OSSL_DISPATCH ep_provider_functions[] = {
	{ OSSL_FUNC_PROVIDER_TEARDOWN, (void (*)(void))ep_teardown },
	{ OSSL_FUNC_PROVIDER_GETTABLE_PARAMS, (void (*)(void))ep_prov_gettable_params },
	{ OSSL_FUNC_PROVIDER_GET_PARAMS, (void (*)(void))ep_prov_get_params },
	{ OSSL_FUNC_PROVIDER_QUERY_OPERATION, (void (*)(void))ep_query },
	{ 0, NULL }
};

extern unsigned int OPENSSL_ia32cap_P[4]; /* libcrypto's own dispatch, set at its init */

/* The entry point registered with OSSL_PROVIDER_add_builtin(NULL, "errand", ...). */
int
OSSL_provider_init_errand(const OSSL_CORE_HANDLE *handle, const OSSL_DISPATCH *in, const OSSL_DISPATCH **out, void **provctx)
{
	const char *mode = getenv("TLS_ERRAND");

	local_mode  = mode && !strcmp(mode, "local");
	sign_mode   = mode && (!strcmp(mode, "sign") || !strcmp(mode, "sign+records"));
	cipher_mode = !mode || strcmp(mode, "sign"); /* every other value, as before Phase 3 */
	sign_check  = getenv("TLS_ERRAND_CHECK") != NULL;
	/* the local arm must run the stock arm's assembly path (AES-NI + PCLMUL) or nothing */
	if (local_mode && !((OPENSSL_ia32cap_P[1] >> (57 - 32)) & 1 && (OPENSSL_ia32cap_P[1] >> (33 - 32)) & 1)) {
		fprintf(stderr, "errand provider: no AES-NI/PCLMUL for the local arm\n");
		return 0;
	}
	if (!ebuf_map()) {
		fprintf(stderr, "errand provider: cannot map /dev/errandbuf (is errandbuf.ko loaded?)\n");
		return 0;
	}
	*out = ep_provider_functions;
	*provctx = (void *)ebuf;

	return 1;
}

/* For the client programs: how many SEAL/OPEN records this process has processed (errands, or
 * local EVP calls in local mode). */
unsigned long
errand_provider_records(void)
{
	return errand_records;
}

/* For the client programs: how many SIGN errands this process has issued. */
unsigned long
errand_provider_signs(void)
{
	return errand_signs;
}
