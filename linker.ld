ENTRY(_start)

SECTIONS
{
    /* Load kernel to 1 MiB */
    . = 0x100000;

    /* multiboot header must be first thing in kernel image */
    .text : ALIGN(4096)
    {
        KEEP(*(.multiboot_header))
        KEEP(*(.text._start))
        *(.text*)
    }

    .rodata : ALIGN(4096)
    {
        *(.rodata*)
    }

    .data : ALIGN(4096)
    {
        *(.data*)
    }

    .bss : ALIGN(4096)
    {
        *(.bss*)
        *(COMMON)
    }

    /DISCARD/ :
    {
        *(.comment)
    }
}
