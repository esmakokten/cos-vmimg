# cos-vmimg -- build Linux guest images for Composite's simple_vmm.
#
#   make image RECIPE=shell     build out/vmlinux-shell.img (+ .manifest)
#   make run   RECIPE=shell     boot that image under QEMU
#   make list                   show available recipes
#   make install DESTDIR=<dir>  install the image where simple_vmm expects it
#
# See README.md for the pipeline and for how this differs from a normal distro.

RECIPE ?= shell
KVER   ?= 6.6.155
JOBS   ?= $(shell nproc)
CC     ?= gcc

# Reproducible images. Without these the kernel bakes in a build timestamp and
# the cpio bakes in file mtimes, so two builds of the same recipe differ and the
# image hash in the manifest identifies nothing. Override SOURCE_DATE_EPOCH to
# stamp a real date; the default keeps rebuilds comparable.
# Exported, not just set: BusyBox's kconfig reads SOURCE_DATE_EPOCH from the
# environment (scripts/kconfig/confdata.c) and otherwise stamps localtime into
# the banner string compiled into the binary -- which made the initramfs, and so
# the whole image, differ between machines and between BusyBox rebuilds.
export SOURCE_DATE_EPOCH ?= 1700000000
# BusyBox renders its banner timestamp through localtime even when
# SOURCE_DATE_EPOCH is set, so without this the image still varies by the
# builder's timezone. Pin it for the whole build.
export TZ := UTC
export KBUILD_BUILD_TIMESTAMP := $(shell date -u -d @$(SOURCE_DATE_EPOCH) 2>/dev/null)
export KBUILD_BUILD_USER      := cos-vmimg
export KBUILD_BUILD_HOST      := cos-vmimg

TOP   := $(CURDIR)
BUILD := $(TOP)/build
DL    := $(BUILD)/dl
OUT   := $(TOP)/out

RECIPE_FILE := $(TOP)/recipes/$(RECIPE).toml
ifeq ($(wildcard $(RECIPE_FILE)),)
$(error No such recipe '$(RECIPE)'. Available: $(patsubst $(TOP)/recipes/%.toml,%,$(wildcard $(TOP)/recipes/*.toml)))
endif

# Translate the recipe into make variables. Regenerated whenever the recipe
# changes; a malformed recipe fails here rather than producing an empty image.
RECIPE_MK := $(BUILD)/$(RECIPE)/recipe.mk
$(shell mkdir -p $(BUILD)/$(RECIPE))
$(shell $(TOP)/tools/recipe2mk.py $(RECIPE_FILE) > $(RECIPE_MK).tmp 2>$(RECIPE_MK).err \
        && mv $(RECIPE_MK).tmp $(RECIPE_MK) || true)
ifeq ($(wildcard $(RECIPE_MK)),)
$(error $(shell cat $(RECIPE_MK).err))
endif
include $(RECIPE_MK)

RFS       := $(BUILD)/$(RECIPE)/rootfs
# Build stamps live outside $(RFS). Two reasons: the cpio rule normalises mtimes
# across everything in the rootfs for reproducibility, which would back-date the
# stamps and make every target permanently out of date; and nothing that is not
# part of the guest filesystem belongs in the guest filesystem.
STAMPS    := $(BUILD)/$(RECIPE)/stamps
# Version-specific: switching KVER must not reuse a build directory that was
# configured and built for a different kernel.
KBUILD    := $(BUILD)/$(RECIPE)/kernel-$(KVER)
INITRAMFS := $(BUILD)/$(RECIPE)/initramfs.cpio.gz
# Output names carry the kernel version. Without it, building two versions of
# one recipe silently overwrites the previous image and there is no way to tell
# from the filename which kernel you are about to ship.
IMG       := $(OUT)/vmlinux-$(RECIPE)-$(KVER).img
ISO       := $(OUT)/kernel-$(RECIPE)-$(KVER).iso
MANIFEST  := $(OUT)/vmlinux-$(RECIPE)-$(KVER).manifest

include mk/fetch.mk
include mk/kernel.mk
include mk/initramfs.mk
include mk/booter.mk

.PHONY: image all list run run-kvm debug iso install clean clean-all distclean gdb help
.DEFAULT_GOAL := image

$(BUILD) $(OUT):
	@mkdir -p $@

image: $(IMG) $(MANIFEST)
	@echo
	@echo "Image:    $(IMG)"
	@echo "Manifest: $(MANIFEST)"

all: image

# The manifest is what turns a directory of anonymous vmlinux*.img blobs into
# artifacts you can identify six months later.
$(MANIFEST): $(IMG)
	@{ \
	  echo "recipe:      $(RECIPE)"; \
	  echo "description: $(RECIPE_DESC)"; \
	  echo "kernel:      $(KVER) (unmodified upstream)"; \
	  echo "busybox:     $(BB_VER)"; \
	  echo "programs:    $(if $(RECIPE_PROGRAMS),$(RECIPE_PROGRAMS),none)"; \
	  echo "modules:     $(if $(RECIPE_MODULES),$(RECIPE_MODULES),none)"; \
	  echo "init:        $(RECIPE_INIT)"; \
	  echo "fragment:    $(if $(RECIPE_FRAGMENT),$(RECIPE_FRAGMENT),none)"; \
	  echo "config:      sha256:$$(sha256sum $(KBUILD)/.config | cut -c1-16)"; \
	  echo "initramfs:   sha256:$$(sha256sum $(INITRAMFS) | cut -c1-16)"; \
	  echo "image:       sha256:$$(sha256sum $(IMG) | cut -c1-16)"; \
	  echo "image-bytes: $$(stat -c%s $(IMG))"; \
	  echo "overlay:     $$(git -C $(TOP) describe --always --dirty 2>/dev/null || echo unknown)"; \
	  echo "built:       $$(date -u +%Y-%m-%dT%H:%M:%SZ)"; \
	} > $@

list:
	@echo "Available recipes:"
	@for r in $(wildcard $(TOP)/recipes/*.toml); do \
		printf "  %-16s %s\n" "$$(basename $$r .toml)" \
		  "$$(sed -n 's/^description *= *"\(.*\)"/\1/p' $$r)"; \
	done

# --- QEMU ---------------------------------------------------------------------
QEMU_ARGS := -chardev null,id=debug \
             -device isa-debugcon,iobase=0xe9,chardev=debug \
             -device edu
MEM   ?= 1024
VCPUS ?= 1

run: $(ISO)
	qemu-system-x86_64 $(QEMU_ARGS) -enable-kvm -cpu max -smp $(VCPUS) \
		-m $(MEM) -cdrom $(ISO) -no-reboot -s -nographic

run-kvm: run

debug: $(ISO)
	@echo "Waiting for gdb on :1234 -- run 'make gdb' in another terminal."
	qemu-system-x86_64 $(QEMU_ARGS) -enable-kvm -cpu max -smp $(VCPUS) \
		-m $(MEM) -cdrom $(ISO) -no-reboot -s -S -nographic

gdb:
	gdb -ex "file $(KBUILD)/vmlinux" -ex "target remote :1234"

iso: $(ISO)

# --- installation into Composite ----------------------------------------------
# DESTDIR is simple_vmm/vmm/guest/. The installed file keeps its full name --
# vmlinux-<recipe>-<kver>.img -- so several images can coexist there and a
# composition script can name the one it wants. Flattening them all to
# vmlinux.img threw away the only thing distinguishing them.
DESTDIR ?=
install: $(IMG) $(MANIFEST)
	@test -n "$(DESTDIR)" || { echo "usage: make install DESTDIR=<.../simple_vmm/vmm/guest>"; exit 1; }
	@test -d "$(DESTDIR)" || { echo "ERROR: $(DESTDIR) is not a directory"; exit 1; }
	@install -m 0644 $(IMG) $(DESTDIR)/$(notdir $(IMG))
	@install -m 0644 $(MANIFEST) $(DESTDIR)/$(notdir $(MANIFEST))
	@echo "Installed -> $(DESTDIR)/$(notdir $(IMG))"

clean:
	rm -rf $(BUILD)/$(RECIPE) $(IMG) $(ISO) $(MANIFEST)

# Everything a recipe build produces, BusyBox included. `clean` deliberately
# keeps BusyBox because rebuilding it is slow -- but that means `clean` is not
# enough to test reproducibility, since a cached BusyBox hides any
# nondeterminism in its own build. Use this for that.
clean-all: clean
	rm -rf $(BB_SRC)

# Keeps the download cache; refetching a 127MB tarball to test a Makefile edit
# is not a good time.
distclean:
	rm -rf $(BUILD)/linux-$(KVER) $(BUILD)/busybox-$(BB_VER) \
	       $(foreach r,$(patsubst $(TOP)/recipes/%.toml,%,$(wildcard $(TOP)/recipes/*.toml)),$(BUILD)/$(r)) \
	       $(OUT)

help:
	@sed -n '2,9p' $(firstword $(MAKEFILE_LIST)) | sed 's/^# \?//'
