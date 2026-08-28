# Initramfs: static BusyBox + the tracked rootfs overlay + the recipe's payload.
#
# Every path under $(RFS) is generated. Nothing here is hand-maintained, so no
# rule ever has to rm -rf a directory that might contain someone's edits -- the
# failure mode that used to eat rootfs/init on every build.

BB_VER      := 1.37.0
BB_TARBALL  := busybox-$(BB_VER).tar.bz2
BB_URL      := https://busybox.net/downloads/$(BB_TARBALL)
# Fallback uses the git snapshot, whose tag spells the version with underscores.
# Using one version string for both URLs is what made the old fetch 404.
BB_URL_ALT  := https://git.busybox.net/busybox/snapshot/busybox-$(subst .,_,$(BB_VER)).tar.bz2
BB_SHA256   := 3311dff32e746499f4df0d5df04d7eb396382d7e108bb9250e7b519b837043a4
BB_SRC      := $(BUILD)/busybox-$(BB_VER)

# BusyBox is recipe-independent, so it is built once and shared.
$(BB_SRC)/busybox: | $(BUILD)
	$(call fetch_verify,$(BB_TARBALL),$(BB_URL),$(BB_SHA256),$(BB_URL_ALT))
	@rm -rf $(BB_SRC)
	@echo "  [TAR]   $(BB_TARBALL)"
	@tar -xf $(DL)/$(BB_TARBALL) -C $(BUILD)
	@echo "  [CONF]  busybox (static)"
	@$(MAKE) -s -C $(BB_SRC) defconfig >/dev/null
	@sed -i 's/^# CONFIG_STATIC is not set/CONFIG_STATIC=y/' $(BB_SRC)/.config
	@sed -i 's/^CONFIG_TC=y/# CONFIG_TC is not set/' $(BB_SRC)/.config
	@$(MAKE) -s -C $(BB_SRC) oldconfig </dev/null >/dev/null
	@echo "  [BB]    building busybox $(BB_VER)"
	@$(MAKE) -s -C $(BB_SRC) -j$(JOBS)
	@file $(BB_SRC)/busybox | grep -q 'statically linked' || { \
		echo "ERROR: busybox is not static; the image has no dynamic loader."; \
		exit 1; }

RECIPE_PROG_BINS := $(patsubst %.c,$(RFS)/programs/%,$(RECIPE_PROGRAMS))
RECIPE_MOD_OBJS  := $(patsubst %.c,%.ko,$(RECIPE_MODULES))

$(RFS)/.busybox-installed: $(BB_SRC)/busybox
	@rm -rf $(RFS)
	@mkdir -p $(RFS)
	@$(MAKE) -s -C $(BB_SRC) install CONFIG_PREFIX=$(abspath $(RFS)) >/dev/null
	@mkdir -p $(RFS)/proc $(RFS)/sys $(RFS)/dev $(RFS)/etc
	@touch $@

# The tracked overlay is applied AFTER the BusyBox install, so rootfs/ always
# wins. This is the whole point of tracking it.
$(RFS)/.overlay: $(RFS)/.busybox-installed $(shell find $(TOP)/rootfs -type f)
	@echo "  [FS]    overlay rootfs/"
	@cp -a $(TOP)/rootfs/. $(RFS)/
	@chmod +x $(RFS)/init
	@touch $@

$(RFS)/programs/%: $(TOP)/programs/%.c $(RFS)/.overlay
	@mkdir -p $(RFS)/programs
	@echo "  [CC]    $* (static)"
	@$(CC) -static -O2 -Wall -o $@ $< -lm

# Modules are built against the same configured kernel the image will run.
$(RFS)/.modules: $(RFS)/.overlay $(KBUILD)/.config
ifneq ($(RECIPE_MODULES),)
	@mkdir -p $(RFS)/modules $(BUILD)/$(RECIPE)/modsrc
	@cp $(addprefix $(TOP)/programs/modules/,$(RECIPE_MODULES)) $(BUILD)/$(RECIPE)/modsrc/
	@printf '%s\n' $(addprefix obj-m += ,$(RECIPE_MOD_OBJS)) > $(BUILD)/$(RECIPE)/modsrc/Kbuild
	@echo "  [KMOD]  $(RECIPE_MODULES)"
	@$(MAKE) -s -C $(LINUX_SRC) O=$(abspath $(KBUILD)) \
		M=$(abspath $(BUILD)/$(RECIPE)/modsrc) modules
	@cp $(BUILD)/$(RECIPE)/modsrc/*.ko $(RFS)/modules/
endif
	@touch $@

$(RFS)/.payload: $(RFS)/.overlay $(TOP)/programs/scripts/$(RECIPE_INIT)
	@echo "  [FS]    payload $(RECIPE_INIT) -> /recipe-init"
	@cp $(TOP)/programs/scripts/$(RECIPE_INIT) $(RFS)/recipe-init
	@chmod +x $(RFS)/recipe-init
	@touch $@

INITRAMFS_DEPS := $(RFS)/.overlay $(RFS)/.payload $(RECIPE_PROG_BINS)
ifneq ($(RECIPE_MODULES),)
INITRAMFS_DEPS += $(RFS)/.modules
endif

$(INITRAMFS): $(INITRAMFS_DEPS)
	@echo "  [CPIO]  $@"
	@cd $(RFS) && find . -print0 \
		| cpio --null --quiet -o -H newc \
		| gzip -9 > $(abspath $@)
