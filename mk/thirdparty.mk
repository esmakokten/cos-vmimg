# Third-party code a recipe can link into its programs: `libs = ["bearssl"]` in the recipe.
# Every tarball is fetched through fetch_verify with a pinned sha256 and built once under
# build/, static, for the guest's own x86-64.
#
# Phase 2 of w5_tls_errand adds nginx and OpenSSL here (the `overlay` recipe key).

THIRDPARTY_DEPS   :=
THIRDPARTY_CFLAGS :=
THIRDPARTY_LIBS   :=

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

UNKNOWN_LIBS := $(filter-out bearssl,$(RECIPE_LIBS))
ifneq ($(UNKNOWN_LIBS),)
$(error recipe names unknown libs: $(UNKNOWN_LIBS))
endif
