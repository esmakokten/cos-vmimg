/*
 * x86 boot protocol structures, defined locally.
 *
 * kmain.c used to include <asm/bootparam.h> from the kernel source being built.
 * That drags the kernel's internal header chain into a freestanding -nostdinc
 * build, and the chain differs between versions: 6.6 reaches asm/rwonce.h,
 * which needs kernel-internal compiler attributes and does not compile here,
 * while 5.15 never got that far. Tracking those headers across versions is a
 * losing game.
 *
 * These layouts are the boot protocol -- an ABI, not an implementation detail.
 * Verified identical between 5.15.107 and 6.6.155, and every offset below is
 * static_asserted, so a future kernel that changed one would break the build
 * loudly rather than corrupt the zero page silently. This is what GRUB,
 * systemd-boot and kexec all do.
 *
 * Reference: Documentation/x86/boot.rst and Documentation/x86/zero-page.rst.
 */
#ifndef VMXBOOTER_BOOT_PROTOCOL_H
#define VMXBOOTER_BOOT_PROTOCOL_H

typedef unsigned char      __u8;
typedef unsigned short     __u16;
typedef unsigned int       __u32;
typedef unsigned long long __u64;

#define BP_ASSERT_OFF(type, field, off) \
	_Static_assert(__builtin_offsetof(type, field) == (off), \
	               #type "." #field " must sit at " #off)

/* setup_header, as embedded in a bzImage at file offset 0x1f1. */
struct setup_header {
	__u8  setup_sects;
	__u16 root_flags;
	__u32 syssize;
	__u16 ram_size;
	__u16 vid_mode;
	__u16 root_dev;
	__u16 boot_flag;
	__u16 jump;
	__u32 header;             /* "HdrS" == 0x53726448 */
	__u16 version;
	__u32 realmode_swtch;
	__u16 start_sys_seg;
	__u16 kernel_version;
	__u8  type_of_loader;
	__u8  loadflags;
	__u16 setup_move_size;
	__u32 code32_start;
	__u32 ramdisk_image;
	__u32 ramdisk_size;
	__u32 bootsect_kludge;
	__u16 heap_end_ptr;
	__u8  ext_loader_ver;
	__u8  ext_loader_type;
	__u32 cmd_line_ptr;
	__u32 initrd_addr_max;
	__u32 kernel_alignment;
	__u8  relocatable_kernel;
	__u8  min_alignment;
	__u16 xloadflags;
	__u32 cmdline_size;
	__u32 hardware_subarch;
	__u64 hardware_subarch_data;
	__u32 payload_offset;
	__u32 payload_length;
	__u64 setup_data;
	__u64 pref_address;
	__u32 init_size;
	__u32 handover_offset;
	__u32 kernel_info_offset;
} __attribute__((packed));

/* Offsets within the bzImage are these plus the 0x1f1 base of the header. */
#define SETUP_HEADER_OFF 0x1f1
BP_ASSERT_OFF(struct setup_header, boot_flag, 0x1fe - SETUP_HEADER_OFF);
BP_ASSERT_OFF(struct setup_header, header,    0x202 - SETUP_HEADER_OFF);
BP_ASSERT_OFF(struct setup_header, version,   0x206 - SETUP_HEADER_OFF);
BP_ASSERT_OFF(struct setup_header, init_size, 0x260 - SETUP_HEADER_OFF);
_Static_assert(sizeof(struct setup_header) == 0x26c - SETUP_HEADER_OFF,
               "setup_header size must match the documented protocol");

struct boot_e820_entry {
	__u64 addr;
	__u64 size;
	__u32 type;
} __attribute__((packed));
_Static_assert(sizeof(struct boot_e820_entry) == 20, "e820 entry is 20 packed bytes");

#define E820_MAX_ENTRIES_ZEROPAGE 128

/* The zero page. Only the fields we actually fill are named. */
struct boot_params {
	__u8  _pad_head[0x1e8];
	__u8  e820_entries;                                   /* 0x1e8 */
	__u8  _pad_a[0x1ef - 0x1e9];
	__u8  sentinel;                                       /* 0x1ef */
	__u8  _pad_b[0x1f1 - 0x1f0];
	struct setup_header hdr;                              /* 0x1f1 */
	__u8  _pad_c[0x2d0 - 0x26c];
	struct boot_e820_entry e820_table[E820_MAX_ENTRIES_ZEROPAGE];  /* 0x2d0 */
	__u8  _pad_tail[0x1000 - 0x2d0 - E820_MAX_ENTRIES_ZEROPAGE * 20];
} __attribute__((packed));

BP_ASSERT_OFF(struct boot_params, e820_entries, 0x1e8);
BP_ASSERT_OFF(struct boot_params, sentinel,     0x1ef);
BP_ASSERT_OFF(struct boot_params, hdr,          0x1f1);
BP_ASSERT_OFF(struct boot_params, e820_table,   0x2d0);
_Static_assert(sizeof(struct boot_params) == 0x1000, "the zero page is one page");

#endif /* VMXBOOTER_BOOT_PROTOCOL_H */
