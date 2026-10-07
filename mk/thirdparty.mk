# Third-party code a recipe can link into its programs: `libs = ["bearssl"]` in the recipe.
# Every tarball is fetched through fetch_verify with a pinned sha256 and built once under
# build/, static, for the guest's own x86-64.
#
# Phase 2 of w5_tls_errand adds nginx and OpenSSL here (the `overlay` recipe key).

THIRDPARTY_DEPS        :=
THIRDPARTY_CFLAGS      :=
THIRDPARTY_LIBS        :=
THIRDPARTY_ROOTFS      :=
THIRDPARTY_ROOTFS_DEPS :=
THIRDPARTY_OBJS        :=

# --- BearSSL 0.6: the in-guest arm of the TLS sweep (the same code the tlskey service runs)
BEARSSL_VER     := 0.6
BEARSSL_TARBALL := bearssl-$(BEARSSL_VER).tar.gz
BEARSSL_URL     := https://bearssl.org/$(BEARSSL_TARBALL)
BEARSSL_SHA256  := 6705bba1714961b41a728dfc5debbe348d2966c117649392f8c8139efc83ff14
BEARSSL_SRC     := $(BUILD)/bearssl-$(BEARSSL_VER)
BEARSSL_LIB     := $(BEARSSL_SRC)/libbearssl.a
# -maes -mpclmul -msse4.1: the AES-NI block ciphers and the PCLMUL GHASH, as in Composite's
# lib/bearssl. Callers select br_aes_x86ni_* and br_ghash_pclmul explicitly.
BEARSSL_CFLAGS  := -O2 -maes -mpclmul -msse4.1 -mssse3 -fPIE -I$(BEARSSL_SRC)/inc -I$(BEARSSL_SRC)/src

$(BEARSSL_SRC)/.extracted: | $(BUILD)
	$(call fetch_verify,$(BEARSSL_TARBALL),$(BEARSSL_URL),$(BEARSSL_SHA256),)
	@rm -rf $(BEARSSL_SRC)
	@echo "  [TAR]   $(BEARSSL_TARBALL)"
	@tar -xzf $(DL)/$(BEARSSL_TARBALL) -C $(BUILD)
	@touch $@

$(BEARSSL_LIB): $(BEARSSL_SRC)/.extracted
	@echo "  [CC]    bearssl $(BEARSSL_VER) (static, AES-NI + PCLMUL)"
	@cd $(BEARSSL_SRC) && for f in $$(find src -name '*.c' | LC_ALL=C sort); do \
		$(CC) $(BEARSSL_CFLAGS) -c -o $${f%.c}.o $$f || exit 1; done
	@cd $(BEARSSL_SRC) && ar rcs $@ $$(find src -name '*.o' | LC_ALL=C sort)

ifneq ($(filter bearssl,$(RECIPE_LIBS)),)
THIRDPARTY_DEPS   += $(BEARSSL_LIB)
THIRDPARTY_CFLAGS += -I$(BEARSSL_SRC)/inc
THIRDPARTY_LIBS   += $(BEARSSL_LIB)
endif


# --- OpenSSL 3.0.17, static (no-shared no-dso no-module): tlsbench links it, nginx builds on it
OPENSSL_VER     := 3.0.17
OPENSSL_TARBALL := openssl-$(OPENSSL_VER).tar.gz
OPENSSL_URL     := https://github.com/openssl/openssl/releases/download/openssl-$(OPENSSL_VER)/$(OPENSSL_TARBALL)
# from $(OPENSSL_URL).sha256
OPENSSL_SHA256  := dfdd77e4ea1b57ff3a6dbde6b0bdc3f31db5ac99e7fdd4eaf9e1fbb6ec2db8ce
OPENSSL_SRC     := $(BUILD)/openssl-$(OPENSSL_VER)
OPENSSL_PREFIX  := $(BUILD)/openssl-$(OPENSSL_VER)-install
OPENSSL_LIB     := $(OPENSSL_PREFIX)/lib/libssl.a

$(OPENSSL_SRC)/.extracted: | $(BUILD)
	$(call fetch_verify,$(OPENSSL_TARBALL),$(OPENSSL_URL),$(OPENSSL_SHA256),)
	@rm -rf $(OPENSSL_SRC)
	@echo "  [TAR]   $(OPENSSL_TARBALL)"
	@tar -xzf $(DL)/$(OPENSSL_TARBALL) -C $(BUILD)
	@touch $@

$(OPENSSL_LIB): $(OPENSSL_SRC)/.extracted
	@echo "  [CC]    openssl $(OPENSSL_VER) (static)"
	@cd $(OPENSSL_SRC) && ./Configure linux-x86_64 no-shared no-dso no-module no-tests \
		--prefix=$(OPENSSL_PREFIX) --libdir=lib > configure.log 2>&1
	@$(MAKE) -s -C $(OPENSSL_SRC) -j$(JOBS) > $(OPENSSL_SRC)/build.log 2>&1
	@$(MAKE) -s -C $(OPENSSL_SRC) install_sw > $(OPENSSL_SRC)/install.log 2>&1

ifneq ($(filter openssl,$(RECIPE_LIBS)),)
THIRDPARTY_DEPS   += $(OPENSSL_LIB)
THIRDPARTY_CFLAGS += -I$(OPENSSL_PREFIX)/include
THIRDPARTY_LIBS   += -L$(OPENSSL_PREFIX)/lib -lssl -lcrypto
endif

# --- nginx 1.26.3, static, on the OpenSSL above; staged under /nginx (prefix) and copied
# into the rootfs when the recipe lists it. nginx.org publishes PGP signatures, not sha256;
# this value was taken at first fetch (2026-10-06) and pins the tarball from then on.
NGINX_VER     := 1.26.3
NGINX_TARBALL := nginx-$(NGINX_VER).tar.gz
NGINX_URL     := https://nginx.org/download/$(NGINX_TARBALL)
NGINX_SHA256  := 69ee2b237744036e61d24b836668aad3040dda461fe6f570f1787eab570c75aa
NGINX_SRC     := $(BUILD)/nginx-$(NGINX_VER)
NGINX_STAGE   := $(BUILD)/nginx-$(NGINX_VER)-stage
NGINX_BIN     := $(NGINX_STAGE)/nginx/sbin/nginx
# The errand provider is linked into nginx (and into programs whose recipe lists
# `errand-provider`); the patch registers it when TLS_ERRAND is set.
ERRAND_PROVIDER_SRC := $(TOP)/thirdparty/errand-provider/errand_provider.c
ERRAND_PROVIDER_OBJ := $(BUILD)/errand_provider.o
ERRAND_PROVIDER_LIB := $(BUILD)/liberrandprov.a
$(ERRAND_PROVIDER_OBJ): $(ERRAND_PROVIDER_SRC) $(wildcard $(TOP)/programs/*.h) $(OPENSSL_LIB) $(BEARSSL_LIB) | $(BUILD)
	@echo "  [CC]    errand_provider.o"
	@$(CC) -O2 -Wall -I$(TOP)/programs -I$(OPENSSL_PREFIX)/include -I$(BEARSSL_SRC)/inc -c -o $@ $<
# An archive, not a bare object: nginx's configure link-tests --with-ld-opt without libcrypto,
# and a bare object's OpenSSL references would fail that test. A member is pulled only when
# nginx.c references OSSL_provider_init_errand, and the libraries repeated after it resolve
# the member's references (static link order).
$(ERRAND_PROVIDER_LIB): $(ERRAND_PROVIDER_OBJ)
	@rm -f $@ && ar rcs $@ $<
NGINX_EXTRA_LDFLAGS := -L$(BUILD) -lerrandprov $(BEARSSL_LIB) -lssl -lcrypto
NGINX_EXTRA_DEPS    := $(ERRAND_PROVIDER_LIB) $(BEARSSL_LIB)
NGINX_PATCHES       := $(TOP)/thirdparty/nginx/0001-errand-provider.patch

$(NGINX_SRC)/.extracted: $(NGINX_PATCHES) | $(BUILD)
	$(call fetch_verify,$(NGINX_TARBALL),$(NGINX_URL),$(NGINX_SHA256),)
	@rm -rf $(NGINX_SRC)
	@echo "  [TAR]   $(NGINX_TARBALL)"
	@tar -xzf $(DL)/$(NGINX_TARBALL) -C $(BUILD)
	@for p in $(NGINX_PATCHES); do echo "  [PATCH] $$p"; patch -d $(NGINX_SRC) -p1 < $$p || exit 1; done
	@touch $@

$(NGINX_BIN): $(NGINX_SRC)/.extracted $(OPENSSL_LIB) $(NGINX_EXTRA_DEPS)
	@echo "  [CC]    nginx $(NGINX_VER) (static, http_ssl)"
	@cd $(NGINX_SRC) && ./configure --prefix=/nginx --with-http_ssl_module \
		--without-http_rewrite_module --without-http_gzip_module \
		--with-cc-opt="-O2 -I$(OPENSSL_PREFIX)/include" \
		--with-ld-opt="-static -L$(OPENSSL_PREFIX)/lib $(NGINX_EXTRA_LDFLAGS)" > configure.log 2>&1
	@$(MAKE) -s -C $(NGINX_SRC) -j$(JOBS) > $(NGINX_SRC)/build.log 2>&1
	@rm -rf $(NGINX_STAGE)
	@$(MAKE) -s -C $(NGINX_SRC) install DESTDIR=$(NGINX_STAGE) > $(NGINX_SRC)/install.log 2>&1
	@strip $@
	@file $@ | grep -q 'statically linked' || { echo "ERROR: nginx is not static; the image has no dynamic loader."; exit 1; }

ifneq ($(filter errand-provider,$(RECIPE_LIBS)),)
THIRDPARTY_DEPS   += $(ERRAND_PROVIDER_LIB)
# prepended: the archive must precede -lssl -lcrypto, and as an archive it is pulled only by
# the programs that reference the provider (a bare object dragged libcrypto into every one)
THIRDPARTY_LIBS   := -L$(BUILD) -lerrandprov $(THIRDPARTY_LIBS)
endif

ifneq ($(filter nginx,$(RECIPE_LIBS)),)
THIRDPARTY_ROOTFS_DEPS += $(NGINX_BIN)
THIRDPARTY_ROOTFS      += $(NGINX_STAGE)
endif

UNKNOWN_LIBS := $(filter-out bearssl openssl nginx errand-provider,$(RECIPE_LIBS))
ifneq ($(UNKNOWN_LIBS),)
$(error recipe names unknown libs: $(UNKNOWN_LIBS))
endif
