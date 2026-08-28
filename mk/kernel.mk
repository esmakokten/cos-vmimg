# Stock kernel source + our three patches, built out-of-tree.
#
# The source tree stays pristine apart from patches/, so it is shared by every
# recipe; per-recipe configuration lives entirely in the O= build directory.

LINUX_TARBALL := linux-$(KVER).tar.xz
LINUX_URL     := https://cdn.kernel.org/pub/linux/kernel/v5.x/$(LINUX_TARBALL)
LINUX_SHA256  := 19370e769045681f52cceedb14ecda97e89b1b058133a0c8ad45d35ffbc5afa8
LINUX_SRC     := $(BUILD)/linux-$(KVER)

PATCHES := $(sort $(wildcard $(TOP)/patches/*.patch))

$(LINUX_SRC)/.patched: $(PATCHES) | $(BUILD)
	$(call fetch_verify,$(LINUX_TARBALL),$(LINUX_URL),$(LINUX_SHA256),)
	@rm -rf $(LINUX_SRC)
	@echo "  [TAR]   $(LINUX_TARBALL)"
	@tar -xf $(DL)/$(LINUX_TARBALL) -C $(BUILD)
	@for p in $(PATCHES); do \
		echo "  [PATCH] $$(basename $$p)"; \
		patch -p1 -d $(LINUX_SRC) -i $$p >/dev/null; \
	done
	@cp $(TOP)/configs/vmxbooter_defconfig $(LINUX_SRC)/arch/x86/configs/
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
$(KBUILD)/.config: $(LINUX_SRC)/.patched $(TOP)/configs/vmxbooter_defconfig
	@mkdir -p $(KBUILD)
	@echo "  [CONF]  vmxbooter_defconfig"
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

$(VMLINUX_BIN): $(KBUILD)/.config $(INITRAMFS)
	@echo "  [KERN]  building $(KVER) for recipe '$(RECIPE)'"
	@$(MAKE) -s -C $(LINUX_SRC) O=$(abspath $(KBUILD)) -j$(JOBS) bzImage
