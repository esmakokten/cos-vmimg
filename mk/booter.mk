# vmxbooter: a multiboot2 trampoline that fabricates the boot environment Linux
# would normally get from firmware, with the kernel image .incbin'd into it.
#
# Output is out/vmlinux-<recipe>.img -- the flat binary Composite's simple_vmm
# INCBINs. See README for why there are four nested embeddings.

BOOTER_CC := $(CC) -m64
BOOTER_LD := ld -m elf_x86_64 --nmagic

# The booter no longer includes any kernel header: the boot protocol structs it
# needs are vendored in vmxbooter/boot_protocol.h with static_asserted offsets.
# That is what lets the same loader build against any kernel version.
BOOTER_INC := -I$(TOP)/vmxbooter

BOOTER_WARN := -Wall -Wcast-align -Wformat=2 -Winit-self -Wmissing-declarations \
               -Wmissing-prototypes -Wnested-externs -Wno-system-headers \
               -Wold-style-definition -Wredundant-decls -Wsign-compare \
               -Wstrict-prototypes -Wundef -Wvolatile-register-var \
               -Wwrite-strings -Wno-address-of-packed-member

BOOTER_CFLAGS := -g3 -O3 -ffreestanding -nostdinc -nostdlib -fno-pic \
                 -mno-red-zone -mcmodel=large -mno-sse -mno-sse2 \
                 -mgeneral-regs-only \
                 -Wno-unused-function -Wno-unused-variable \
                 -Wno-unused-but-set-variable $(BOOTER_WARN)

BDIR := $(BUILD)/$(RECIPE)/vmxbooter

# guest_img.S does `.incbin "bzImage"` relative to its own directory, so the
# kernel image is staged next to it rather than referenced by an absolute path.
#
# We embed the full bzImage, not the raw compressed vmlinux, so kmain.c can read
# setup_sects and init_size out of the setup header instead of hardcoding them.
$(BDIR)/bzImage: $(BZIMAGE)
	@mkdir -p $(BDIR)
	@cp $< $@

$(BDIR)/guest_img.o: $(TOP)/vmxbooter/guest_img.S $(BDIR)/bzImage
	@echo "  [AS]    guest_img.o (embedding $(KVER) bzImage)"
	@$(BOOTER_CC) -c -o $@ -I$(BDIR) $<

$(BDIR)/loader.o: $(TOP)/vmxbooter/loader.S | $(BDIR)
	@echo "  [AS]    loader.o"
	@$(BOOTER_CC) -c $(BOOTER_INC) -o $@ $<

# Depends on the built kernel, not merely on .config: kmain.c includes
# <asm/bootparam.h>, which pulls in generated uapi headers that only exist after
# the kernel's archprepare step has run. With .config as the prerequisite this
# compiles fine serially -- the kernel build always happens to precede it -- and
# fails under -j with "asm/types.h: No such file or directory".
$(BDIR)/kmain.o: $(TOP)/vmxbooter/kmain.c $(BZIMAGE) | $(BDIR)
	@echo "  [CC]    kmain.o"
	@$(BOOTER_CC) $(BOOTER_INC) $(BOOTER_CFLAGS) -c -o $@ $<

$(BDIR):
	@mkdir -p $@

# linker.ld places the 32-bit entry stub with `loader.o(.text)`, which is a
# filename *pattern* matched against the input names as spelled on the command
# line. Absolute paths do not match it, and ld then tries to open "loader.o" as
# a file and fails. So stage the script beside the objects and link from there
# with bare names, exactly as the original in-tree build did -- the section
# layout this produces is what kmain.c's load addresses assume.
$(BDIR)/kernel.img: $(TOP)/vmxbooter/linker.ld $(BDIR)/loader.o $(BDIR)/guest_img.o $(BDIR)/kmain.o
	@echo "  [LD]    kernel.img"
	@cp $(TOP)/vmxbooter/linker.ld $(BDIR)/linker.ld
	@cd $(BDIR) && $(BOOTER_LD) -T linker.ld loader.o guest_img.o kmain.o -o kernel.img

$(IMG): $(BDIR)/kernel.img | $(OUT)
	@echo "  [IMG]   $@"
	@objcopy -O binary -R .note -R .comment -S $< $@

# GRUB rescue ISO, for booting the same image under plain QEMU.
$(ISO): $(BDIR)/kernel.img | $(OUT)
	@echo "  [ISO]   $@"
	@$(TOP)/vmxbooter/build_iso.sh $(BDIR)/kernel.img
	@mv $(BDIR)/kernel.iso $@
