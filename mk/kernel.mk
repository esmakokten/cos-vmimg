# Stock kernel source + our three patches, built out-of-tree.
#
# The source tree stays pristine apart from patches/, so it is shared by every
# recipe; per-recipe configuration lives entirely in the O= build directory.

LINUX_TARBALL := linux-$(KVER).tar.xz
LINUX_SRC     := $(BUILD)/linux-$(KVER)

# The tarball lives under v<major>.x, so the URL follows KVER rather than being
# pinned to one series.
KVER_MAJOR := $(firstword $(subst ., ,$(KVER)))
LINUX_URL  := https://cdn.kernel.org/pub/linux/kernel/v$(KVER_MAJOR).x/$(LINUX_TARBALL)

# Known-good checksums, from cdn.kernel.org/pub/linux/kernel/v<major>.x/sha256sums.asc.
# Adding a kernel version means adding a line here: the build refuses to fetch a
# version it has no recorded checksum for rather than trusting whatever the
# network returns.
LINUX_SHA256_5.15.107 := 19370e769045681f52cceedb14ecda97e89b1b058133a0c8ad45d35ffbc5afa8
LINUX_SHA256_6.1.186  := eeedc32bbf2448205aff50ee2760a4d87172cf8f8279c1e5930069ad36f6236e
LINUX_SHA256_6.6.155  := 4e67a9263f2c19b070112109c9a282ee8e8ea49f1641e41faa5cca2c41654982

LINUX_SHA256 := $(LINUX_SHA256_$(KVER))
ifeq ($(LINUX_SHA256),)
$(error No sha256 recorded for kernel $(KVER). Add LINUX_SHA256_$(KVER) to mk/kernel.mk, taking the value from https://cdn.kernel.org/pub/linux/kernel/v$(KVER_MAJOR).x/sha256sums.asc)
endif

$(LINUX_SRC)/.extracted: | $(BUILD)
	$(call fetch_verify,$(LINUX_TARBALL),$(LINUX_URL),$(LINUX_SHA256),)
	@rm -rf $(LINUX_SRC)
	@echo "  [TAR]   $(LINUX_TARBALL)"
	@tar -xf $(DL)/$(LINUX_TARBALL) -C $(BUILD)
	@touch $@

# --- per-recipe kernel configuration -----------------------------------------
# CONFIG_INITRAMFS_SOURCE is given an ABSOLUTE path outside the kernel tree.
# usr/Makefile uses a .cpio.gz value verbatim with compress-y := shipped, so the
# archive is embedded as-is and never has to live inside the kernel source.
# That is what lets this repo stay an overlay.

# NOTE: this deliberately does NOT depend on $(INITRAMFS). Modules are built
# against this configured tree and are themselves part of the initramfs, so a
# dependency here would be circular. CONFIG_INITRAMFS_SOURCE is set to the
# archive's future path; kbuild only needs the file to exist at build time, and
# $(VMLINUX_BIN) below depends on it, which is what actually enforces the order.
$(KBUILD)/.config: $(LINUX_SRC)/.extracted $(TOP)/configs/vmxbooter_defconfig
	@mkdir -p $(KBUILD)
	@echo "  [CONF]  vmxbooter_defconfig"
	@# Copied here, not at extraction time: otherwise editing the defconfig has
	@# no effect until the kernel tree happens to be re-extracted.
	@cp $(TOP)/configs/vmxbooter_defconfig $(LINUX_SRC)/arch/x86/configs/
	@$(MAKE) -s -C $(LINUX_SRC) O=$(abspath $(KBUILD)) vmxbooter_defconfig
ifneq ($(RECIPE_FRAGMENT),)
	@echo "  [CONF]  fragment $(RECIPE_FRAGMENT)"
	@cat $(TOP)/configs/fragments/$(RECIPE_FRAGMENT) >> $(KBUILD)/.config
endif
	@$(LINUX_SRC)/scripts/config --file $(KBUILD)/.config \
		--set-str CONFIG_INITRAMFS_SOURCE "$(abspath $(INITRAMFS))"
	@$(MAKE) -s -C $(LINUX_SRC) O=$(abspath $(KBUILD)) olddefconfig
	@grep -q '^CONFIG_DEVTMPFS=y' $(KBUILD)/.config || { \
		echo "ERROR: CONFIG_DEVTMPFS=y did not survive olddefconfig."; \
		echo "       /init mounts devtmpfs and will fail without it."; exit 1; }

VMLINUX_BIN := $(KBUILD)/arch/x86/boot/vmlinux.bin
BZIMAGE     := $(KBUILD)/arch/x86/boot/bzImage

# --- phase one: scaffold -------------------------------------------------------
# Builds vmlinux and the in-tree modules against a placeholder initramfs, purely
# to produce a populated Module.symvers for external modules to link against.
# Only recipes that actually carry modules need this; for the rest the initramfs
# has no dependency on the kernel and the scaffold is a no-op stamp.
$(KBUILD)/.scaffold: $(KBUILD)/.config
ifneq ($(RECIPE_MODULES),)
	@mkdir -p $(dir $(INITRAMFS))
	@if [ ! -f $(INITRAMFS) ]; then 		echo "  [KERN]  scaffold build (for Module.symvers)"; 		: | cpio --quiet -o -H newc | gzip -9 > $(INITRAMFS); 	fi
	@$(MAKE) -s -C $(LINUX_SRC) O=$(abspath $(KBUILD)) -j$(JOBS) bzImage modules
	@test -s $(KBUILD)/Module.symvers || { 		echo "ERROR: Module.symvers is empty after the scaffold build."; 		echo "       External modules would link with unresolved symbols."; exit 1; }
endif
	@touch $@

# --- phase two: the real kernel ------------------------------------------------
# $(INITRAMFS) here is the real archive, modules included. For a module-carrying
# recipe this is an incremental relink over the scaffold, not a second full build.
$(BZIMAGE): $(KBUILD)/.scaffold $(INITRAMFS)
	@echo "  [KERN]  building $(KVER) for recipe '$(RECIPE)'"
	@$(MAKE) -s -C $(LINUX_SRC) O=$(abspath $(KBUILD)) -j$(JOBS) bzImage
