OUTPUT_FORMAT(elf64-x86-64)
ENTRY (loader)
phys = 0x00100000;
SECTIONS {

    . = phys;

    .boottext : AT(phys) ALIGN(8)
    {
       loader.o(.text)
       . = ALIGN(4096);
    }
    
    phys = .;

    . += 0xffffffff80000000;

    .text : AT(phys) {
        code = .;
        *(.text)
        *(.rodata)
        . = ALIGN(4096);
    }

    .data : AT(phys + (data - code)) {
        data = .;
        *(.data)

        /* The loader's own state (boot_params, cmd line, stack) before the guest image, not
         * after it: the kernel is not relocatable and decompresses to 16 MB (PHYSICAL_START),
         * so anything behind an image that ends above 16 MB is overwritten before the kernel
         * reads boot_params (a 15.9 MB image died in copy_bootdata). The image is last. */
        . = ALIGN(4096);
	bss = .;
        *(.bss)

        . = ALIGN(4096);
        largebss = .;
        *(.largebss)

        . = ALIGN(4096);
        _guest_start = .;
        *(.guest_image)
        _guest_end = .;
        . = ALIGN(4096);
    }
    end = .;
    . = ALIGN(4096);

    /DISCARD/ : {
        *(.comment)
        *(.note.gnu.build-id)
    }
}
