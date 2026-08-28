# pipefail below needs bash, not dash.
SHELL := /bin/bash

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
# Kbuild's obj-m takes the object name, not the module name: obj-m += foo.o
# produces foo.ko.
RECIPE_MOD_OBJS  := $(patsubst %.c,%.o,$(RECIPE_MODULES))

$(STAMPS)/busybox-installed: $(BB_SRC)/busybox
	@rm -rf $(RFS)
	@mkdir -p $(RFS) $(STAMPS)
	@$(MAKE) -s -C $(BB_SRC) install CONFIG_PREFIX=$(abspath $(RFS)) >/dev/null
	@mkdir -p $(RFS)/proc $(RFS)/sys $(RFS)/dev $(RFS)/etc
	@touch $@

# The tracked overlay is applied AFTER the BusyBox install, so rootfs/ always
# wins. This is the whole point of tracking it.
$(STAMPS)/overlay: $(STAMPS)/busybox-installed $(shell find $(TOP)/rootfs -type f)
	@echo "  [FS]    overlay rootfs/"
	@cp -a $(TOP)/rootfs/. $(RFS)/
	@chmod +x $(RFS)/init
	@touch $@

$(RFS)/programs/%: $(TOP)/programs/%.c $(STAMPS)/overlay
	@mkdir -p $(RFS)/programs
	@echo "  [CC]    $* (static)"
	@$(CC) -static -O2 -Wall -o $@ $< -lm

# Modules are built against the very kernel the image will run.
#
# This needs a two-phase kernel build, because the dependency is genuinely
# cyclic: the .ko files go into the initramfs, and the initramfs is linked into
# the kernel. Phase one ($(KBUILD)/.scaffold, in kernel.mk) builds vmlinux and
# the in-tree modules against a placeholder initramfs, which is what produces a
# populated Module.symvers. Without it modpost has no built-in symbol table and
# silently emits .ko files with every external symbol unresolved -- they build
# fine and then fail to insmod. Phase two relinks the kernel around the real
# initramfs and is incremental.
$(STAMPS)/modules: $(STAMPS)/overlay $(KBUILD)/.scaffold
ifneq ($(RECIPE_MODULES),)
	@mkdir -p $(RFS)/modules $(BUILD)/$(RECIPE)/modsrc
	@cp $(addprefix $(TOP)/programs/modules/,$(RECIPE_MODULES)) $(BUILD)/$(RECIPE)/modsrc/
	@: > $(BUILD)/$(RECIPE)/modsrc/Kbuild
	@for o in $(RECIPE_MOD_OBJS); do \
		echo "obj-m += $$o" >> $(BUILD)/$(RECIPE)/modsrc/Kbuild; \
	done
	@echo "  [KMOD]  $(RECIPE_MODULES)"
	@# pipefail matters: piping into tee otherwise reports tee's exit status, so
	@# a module that fails to compile is swallowed and the image ships without
	@# it. This is the same failure-masking the old programs/Makefile had.
	@set -o pipefail; $(MAKE) -s -C $(LINUX_SRC) O=$(abspath $(KBUILD)) \
		M=$(abspath $(BUILD)/$(RECIPE)/modsrc) modules 2>&1 \
		| tee $(BUILD)/$(RECIPE)/modsrc/modpost.log
	@if grep -q 'undefined!' $(BUILD)/$(RECIPE)/modsrc/modpost.log; then \
		echo "ERROR: modules have unresolved symbols and would fail to insmod:"; \
		grep 'undefined!' $(BUILD)/$(RECIPE)/modsrc/modpost.log; \
		exit 1; fi
	@cp $(BUILD)/$(RECIPE)/modsrc/*.ko $(RFS)/modules/
endif
	@touch $@

$(STAMPS)/payload: $(STAMPS)/overlay $(TOP)/programs/scripts/$(RECIPE_INIT)
	@echo "  [FS]    payload $(RECIPE_INIT) -> /recipe-init"
	@cp $(TOP)/programs/scripts/$(RECIPE_INIT) $(RFS)/recipe-init
	@chmod +x $(RFS)/recipe-init
	@touch $@

INITRAMFS_DEPS := $(STAMPS)/overlay $(STAMPS)/payload $(RECIPE_PROG_BINS)
ifneq ($(RECIPE_MODULES),)
INITRAMFS_DEPS += $(STAMPS)/modules
endif

# Sorted names, uid/gid 0, a fixed mtime, and --reproducible so the same inputs
# give the same archive byte for byte. gzip -n omits its own timestamp.
#
# --reproducible (renumber-inodes + ignore-devno) is the load-bearing one: the
# newc format stores each file's real inode number, so without it a rebuilt
# rootfs produces a different archive even when every file is identical -- and
# since the archive is linked into vmlinux, that made the whole image differ.
$(INITRAMFS): $(INITRAMFS_DEPS)
	@echo "  [CPIO]  $@"
	@find $(RFS) -exec touch -h -d @$(SOURCE_DATE_EPOCH) {} +
	@cd $(RFS) && find . | LC_ALL=C sort \
		| cpio --quiet -o -H newc --owner 0:0 --reproducible \
		| gzip -9n > $(abspath $@)
