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
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <openssl/core.h>
#include <openssl/core_dispatch.h>
#include <openssl/core_names.h>
#include <openssl/params.h>
#include <openssl/provider.h>
#include <openssl/evp.h>
#include "libErrand.h"
#include "tlskey_abi.h"

/* The shared buffer, mapped once at provider init. Layout used by this provider:
 *   [0, 64)          key scratch for SETKEY (zeroed after the errand)
 *   [4096, 4096+72K) one record: [nonce 12][aad_len 2][aad][payload (+ tag)]            */
#define EB_KEY_OFF     0UL
#define EB_REC_OFF     4096UL
#define EB_MAX_PAYLOAD (64UL * 1024)

static unsigned char *ebuf;
static unsigned long  ebuf_len;
static unsigned long  errand_records; /* SEAL + OPEN errands issued by this process */

static int
ebuf_map(void)
{
	long len;
	int  fd;

	if (ebuf) return 1;
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
	unsigned long kid;     /* service key id, 0 = none */
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
	if (c->kid) errand_call(TLSKEY_SLOT, TLSKEY_OPKEY(TLSKEY_OP_DELKEY, c->kid), 0, 0, 0);
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
	c->kid = 0;
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

static const OSSL_ALGORITHM *
ep_query(void *provctx, int operation_id, int *no_cache)
{
	*no_cache = 0;

	return operation_id == OSSL_OP_CIPHER ? ep_ciphers : NULL;
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

/* The entry point registered with OSSL_PROVIDER_add_builtin(NULL, "errand", ...). */
int
OSSL_provider_init_errand(const OSSL_CORE_HANDLE *handle, const OSSL_DISPATCH *in, const OSSL_DISPATCH **out, void **provctx)
{
	if (!ebuf_map()) {
		fprintf(stderr, "errand provider: cannot map /dev/errandbuf (is errandbuf.ko loaded?)\n");
		return 0;
	}
	*out = ep_provider_functions;
	*provctx = (void *)ebuf;

	return 1;
}

/* For the client programs: how many SEAL/OPEN errands this process has issued. */
unsigned long
errand_provider_records(void)
{
	return errand_records;
}
