OUTPUT_FORMAT("pei-x86-64")
OUTPUT_ARCH(i386:x86-64)
ENTRY(efi_main)

SECTIONS
{
    . = 0;

    .text : ALIGN(4096) {
        *(.text*)
    }

    .rdata : ALIGN(4096) {
        *(.rdata*)
        *(.rodata*)
    }

    .data : ALIGN(4096) {
        *(.data*)
    }

    .pdata : ALIGN(4096) {
        *(.pdata*)
    }

    .bss (NOLOAD) : ALIGN(4096) {
        *(.bss*)
        *(COMMON)
    }

    .reloc : ALIGN(4096) {
        *(.reloc*)
    }

    /DISCARD/ : {
        *(.note*)
        *(.comment*)
    }
}
