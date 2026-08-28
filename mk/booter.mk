# vmxbooter: a multiboot2 trampoline that fabricates the boot environment Linux
# would normally get from firmware, with the kernel image .incbin'd into it.
#
# Output is out/vmlinux-<recipe>.img -- the flat binary Composite's simple_vmm
# INCBINs. See README for why there are four nested embeddings.

BOOTER_CC := $(CC) -m64
BOOTER_LD := ld -m elf_x86_64 --nmagic

BOOTER_INC := -I$(TOP)/vmxbooter \
              -I$(LINUX_SRC)/include \
              -I$(LINUX_SRC)/arch/x86/include/uapi \
              -I$(LINUX_SRC)/include/uapi \
              -I$(KBUILD)/arch/x86/include/generated/uapi

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

# guest_img.S does `.incbin "vmlinux.bin"` relative to its own directory, so the
# kernel image is staged next to it rather than referenced by an absolute path.
$(BDIR)/vmlinux.bin: $(VMLINUX_BIN)
	@mkdir -p $(BDIR)
	@cp $< $@

$(BDIR)/guest_img.o: $(TOP)/vmxbooter/guest_img.S $(BDIR)/vmlinux.bin
	@echo "  [AS]    guest_img.o (embedding $(KVER) image)"
	@$(BOOTER_CC) -c -o $@ -I$(BDIR) $<

$(BDIR)/loader.o: $(TOP)/vmxbooter/loader.S | $(BDIR)
	@echo "  [AS]    loader.o"
	@$(BOOTER_CC) -c $(BOOTER_INC) -o $@ $<

$(BDIR)/kmain.o: $(TOP)/vmxbooter/kmain.c $(KBUILD)/.config | $(BDIR)
	@echo "  [CC]    kmain.o"
	@$(BOOTER_CC) $(BOOTER_INC) $(BOOTER_CFLAGS) -c -o $@ $<

$(BDIR):
	@mkdir -p $@

$(BDIR)/kernel.img: $(TOP)/vmxbooter/linker.ld $(BDIR)/loader.o $(BDIR)/guest_img.o $(BDIR)/kmain.o
	@echo "  [LD]    kernel.img"
	@$(BOOTER_LD) -T $< $(BDIR)/loader.o $(BDIR)/guest_img.o $(BDIR)/kmain.o -o $@

$(IMG): $(BDIR)/kernel.img | $(OUT)
	@echo "  [IMG]   $@"
	@objcopy -O binary -R .note -R .comment -S $< $@

# GRUB rescue ISO, for booting the same image under plain QEMU.
$(ISO): $(BDIR)/kernel.img | $(OUT)
	@echo "  [ISO]   $@"
	@$(TOP)/vmxbooter/build_iso.sh $(BDIR)/kernel.img
	@mv $(BDIR)/kernel.iso $@
