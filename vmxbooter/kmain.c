#include "boot_protocol.h"

#define NUM_CPU 1

struct acpi_table_rsdp {
	char signature[8];	/* ACPI signature, contains "RSD PTR " */
	__u8 checksum;		/* ACPI 1.0 checksum */
	char oem_id[6];	/* OEM identification */
	__u8 revision;		/* Must be (0) for ACPI 1.0 or (2) for ACPI 2.0+ */
	__u32 rsdt_physical_address;	/* 32-bit physical address of the RSDT */
	__u32 length;		/* Table length in bytes, including header (ACPI 2.0+) */
	__u64 xsdt_physical_address;	/* 64-bit physical address of the XSDT (ACPI 2.0+) */
	__u8 extended_checksum;	/* Checksum of entire table (ACPI 2.0+) */
	__u8 reserved[3];		/* Reserved, must be zero */
}__attribute__((packed));

struct acpi_table_header {
	char signature[4];	/* ASCII table signature */
	__u32 length;		/* Length of table in bytes, including this header */
	__u8 revision;		/* ACPI Specification minor version number */
	__u8 checksum;		/* To make sum of entire table == 0 */
	char oem_id[6];	/* ASCII OEM identification */
	char oem_table_id[8];	/* ASCII OEM table identification */
	__u32 oem_revision;	/* OEM revision number */
	char asl_compiler_id[4];	/* ASCII ASL compiler vendor ID */
	__u32 asl_compiler_revision;	/* ASL compiler version */
}__attribute__((packed));

struct acpi_table_rsdt {
	struct acpi_table_header header;	/* Common ACPI table header */
	__u32 table_offset_entry[1];	/* Array of pointers to ACPI tables */
}__attribute__((packed));

struct acpi_table_madt {
	struct acpi_table_header header;	/* Common ACPI table header */
	__u32 address;		/* Physical address of local APIC */
	__u32 flags;
}__attribute__((packed));

struct acpi_subtable_header {
	__u8 type;
	__u8 length;
}__attribute__((packed));
struct acpi_madt_local_apic {
	struct acpi_subtable_header header;
	__u8 processor_id;	/* ACPI processor id */
	__u8 id;			/* Processor's local APIC id */
	__u32 lapic_flags;
}__attribute__((packed));

struct acpi_madt_io_apic {
	struct acpi_subtable_header header;
	__u8 id;			/* I/O APIC ID */
	__u8 reserved;		/* reserved - must be zero */
	__u32 address;		/* APIC physical address */
	__u32 global_irq_base;	/* Global system interrupt where INTI lines start */
}__attribute__((packed));

struct acpi_madt {
	struct acpi_table_madt madt;
	struct acpi_madt_local_apic lapics[NUM_CPU];
	struct acpi_madt_io_apic ioapic;
} __attribute__((packed));

struct boot_params boot_params;

const char *cmd_line_str = "console=uart8250,io,0x3f8,115200n8 spectre_v2=off random.trust_cpu=on acpi_force_table_verification=true acpi.debug_level=ACPI_DEBUG";
// rng_core.default_quality=1000
void kmain(void);
extern char input_data, input_data_end;
/*
	0x0000'7FFFFFFFFFFF	user upper
	0xffff'800000000000	machine kernel base
	0xffff'ffff80000000	linux kernel base
	0xffff'ffff81000000	linux kernel base -> 16M base address


	ffff80000	000
	    |
	    v
1111 1111 1' 111 1111 10' 00 0000 000' 0 0000 0000'
0x1ff       ' 0x1fe
*/

#define IMAGE_VADDR_START (0xffffffff80000000UL)
#define _va(x) ((unsigned long)(x) + IMAGE_VADDR_START)
#define _pa(x) ((unsigned long)(x) - IMAGE_VADDR_START)

#define ACPI_START_PADDR (0xE0000)
struct acpi_table_rsdp *g_rsdp = (struct acpi_table_rsdp *)_va(ACPI_START_PADDR);
struct acpi_table_rsdt *g_rsdt = (struct acpi_table_rsdt *)_va(((char *)ACPI_START_PADDR + 64));
struct acpi_madt *g_madt = (struct acpi_madt *)_va(((char *)ACPI_START_PADDR + 128));

static __attribute__((unused))
void *memset(void *dst, int b,  unsigned long len)
{
	char *p = dst;

	while (len--)
		*(p++) = b;
	return dst;
}

static __attribute__((unused))
int memcmp(const void *s1, const void *s2, unsigned long n)
{
	unsigned long ofs = 0;
	int c1 = 0;

	while (ofs < n && !(c1 = ((unsigned char *)s1)[ofs] - ((unsigned char *)s2)[ofs])) {
		ofs++;
	}
	return c1;
}

static inline void *
memcpy(void *dst, const void *src, unsigned long count)
{
	const __u8 *s = (const __u8 *)src;
	__u8 *      d = (__u8 *)dst;

	for (; count != 0; count--) *d++ = *s++;

	return dst;
}


static __attribute__((unused))
unsigned long strlen(const char *s)
{
	const char *p = s;

	while (*p) p++;
	return (unsigned long)(p - s);
}

static __u8 compute_checksum(__u8 *buffer, __u32 length)
{
	__u8 *end = buffer + length;
	__u8 sum = 0;

	while (buffer < end)
		sum += *(buffer++);

	return sum;
}

void apic_init(struct acpi_table_rsdt *rsdt);
void acpi_init(struct acpi_table_rsdp *rsdp);

void apic_init(struct acpi_table_rsdt *rsdt)
{
	memset(rsdt, 0, sizeof(*rsdt));
	memset(g_madt, 0, sizeof(g_madt));

	memcpy(&rsdt->header.signature, "RSDT", 4);
	rsdt->header.length = sizeof(struct acpi_table_rsdt);
	rsdt->header.revision = 0;
	memcpy(&rsdt->header.oem_id, "QEMU", 4);
	rsdt->header.oem_revision = 0;
	rsdt->table_offset_entry[0] = _pa(g_madt);
	
	rsdt->header.checksum = 0x100 - compute_checksum((void *)rsdt, rsdt->header.length);

	struct acpi_table_madt *madt = (struct acpi_table_madt *)g_madt;

	memcpy(&madt->header.signature, "APIC", 4);
	madt->header.revision = 0;
	memcpy(&madt->header.oem_id, "QEMU", 4);
	madt->header.oem_revision = 0;
	madt->address = 0xFEE00000;
	madt->flags = 0;

	madt->header.length = sizeof(struct acpi_table_madt) + sizeof(struct acpi_madt_local_apic) * NUM_CPU + sizeof(struct acpi_madt_io_apic) ;

	for (int i = 0; i < NUM_CPU; i++) {
		struct acpi_madt_local_apic *lapic = &g_madt->lapics[i];
		lapic->header.type = 0;
		lapic->header.length = sizeof(struct acpi_madt_local_apic);
		lapic->processor_id = i;
		lapic->id = i;
		lapic->lapic_flags = 1;
	}

	struct acpi_madt_io_apic *ioapic = &g_madt->ioapic;
	ioapic->header.type = 1;
	ioapic->header.length = sizeof(struct acpi_madt_io_apic);
	ioapic->id = 0x03;
	/* set the last page of physical memory from VMM to be io apic page */
	ioapic->address = 0xFEC00000;
	ioapic->global_irq_base = 0;

	madt->header.checksum = 0x100 - compute_checksum((void *)madt, madt->header.length);

}

void acpi_init(struct acpi_table_rsdp *rsdp)
{
	memset(rsdp, 0, sizeof(*rsdp));
	memcpy(&rsdp->signature, "RSD PTR ", 8);
	memcpy(&rsdp->oem_id, "QEMU ", 5);

	rsdp->revision = 0;
	rsdp->rsdt_physical_address = (__u32)_pa(g_rsdt);

	rsdp->checksum = 0x100 - compute_checksum((void *)rsdp, sizeof(struct acpi_table_rsdp));

	apic_init(g_rsdt);
}

/*
 * Guest memory layout. The kernel is loaded at GUEST_LOAD_BASE and must fit,
 * together with the memory it needs before it can read its own memory map,
 * inside the RAM we advertise in the e820 map below.
 */
#define GUEST_LOAD_BASE  (0x900000UL)
#define GUEST_RAM_START  (0x100000UL)
#define GUEST_RAM_SIZE   (60UL * 1024 * 1024)

/*
 * The 64-bit entry point is at +0x200 from the start of the protected-mode
 * kernel. This is stable boot protocol, not a version-specific constant:
 * boot.rst says "64-bit kernel plus 0x200", and head_64.S places startup_64
 * behind a literal `.org 0x200`.
 */
#define PM_ENTRY_OFF     (0x200)

void kmain(void)
{
	char *vga_base = (char *)_va(0xb8000);
	char *bzimage  = &input_data;
	unsigned long bzimage_sz = (unsigned long)(&input_data_end - &input_data);
	char *load_base = (char *)_va(GUEST_LOAD_BASE);

	/*
	 * Read what we need out of the kernel's own setup header rather than
	 * hardcoding it. init_size in particular used to be a fixed 36MB, which
	 * silently becomes wrong on any other kernel build.
	 */
	struct setup_header *hdr = (struct setup_header *)(bzimage + SETUP_HEADER_OFF);
	unsigned long setup_sects = hdr->setup_sects ? hdr->setup_sects : 4;
	unsigned long pm_offset   = (setup_sects + 1) * 512;
	char *pm_kernel           = bzimage + pm_offset;
	unsigned long pm_size     = bzimage_sz - pm_offset;
	unsigned long init_size   = hdr->init_size;

	/* The image is the last data section; we must not copy over ourselves. */
	if (load_base < bzimage) while (1);

	/* Sanity-check the header before trusting anything else in it. */
	if (hdr->boot_flag != 0xAA55) while (1);
	if (hdr->header != 0x53726448 /* "HdrS" */) while (1);
	/* The advertised init window has to fit in the RAM we are about to claim. */
	if (GUEST_LOAD_BASE + init_size > GUEST_RAM_START + GUEST_RAM_SIZE) while (1);

	/*
	 * Zero the whole init window before loading.
	 *
	 * This replaces a patch to arch/x86/boot/compressed/vmlinux.lds.S that
	 * folded .bss and .pgtable into .data so objcopy would materialise them as
	 * zeros inside the flat image. The underlying problem is that we enter at
	 * startup_64 (+0x200) and so skip startup_32, which is the only code that
	 * zeroes the page-table area (`rep stosl` over BOOT_INIT_PGT_SIZE);
	 * alloc_pgt_page() in ident_map_64.c hands out pages without clearing them.
	 * Zeroing here covers .bss, .pgtable and anything else the kernel expects
	 * to find clear, for any kernel version, with no kernel change.
	 */
	memset(load_base, 0, init_size);

	/* Load the protected-mode kernel; the setup sectors are not needed at runtime. */
	memcpy(load_base, pm_kernel, pm_size);

	acpi_init(g_rsdp);

	memset(&boot_params, 0, sizeof(boot_params));
	vga_base[0] = '!';

	/*
	 * Take the setup header from the kernel itself. This is what makes the
	 * boot protocol version self-adjusting -- it used to be asserted as a
	 * hardcoded 0x0215 along with a hand-assembled "HdrS" signature.
	 */
	memcpy(&boot_params.hdr, hdr, sizeof(struct setup_header));

	boot_params.sentinel            = 0xff;
	boot_params.hdr.vid_mode        = 0xFFFF;
	boot_params.hdr.type_of_loader  = 0xFF;
	boot_params.hdr.code32_start    = GUEST_LOAD_BASE;
	boot_params.hdr.loadflags      |= 0x01;  /* LOADED_HIGH */
	boot_params.hdr.loadflags      |= 0x80;  /* CAN_USE_HEAP */
	boot_params.hdr.heap_end_ptr    = 0xe000 - 0x200;
	boot_params.hdr.ramdisk_image   = 0;     /* initramfs is linked into vmlinux */
	boot_params.hdr.ramdisk_size    = 0;
	boot_params.hdr.setup_data      = 0;
	boot_params.hdr.cmd_line_ptr    = _pa(cmd_line_str);
	boot_params.hdr.cmdline_size    = strlen(cmd_line_str);

	boot_params.e820_entries = 3;
	boot_params.e820_table[0].addr = 0;
	boot_params.e820_table[0].size = 0x9e000;
	boot_params.e820_table[0].type = 1;

	boot_params.e820_table[1].addr = 0x9e000;
	boot_params.e820_table[1].size = 0x62000;
	boot_params.e820_table[1].type = 2;

	boot_params.e820_table[2].addr = GUEST_RAM_START;
	boot_params.e820_table[2].size = GUEST_RAM_SIZE;
	boot_params.e820_table[2].type = 1;

	asm volatile("mov %0, %%rsi; jmpq *%1;"
		     :: "r"((unsigned long)_pa(&boot_params)),
		        "r"(GUEST_LOAD_BASE + PM_ENTRY_OFF));
	while (1) {
	}
}
