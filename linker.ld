ENTRY(_start)

SECTIONS
{
    . = 0x100000;

    .text ALIGN(4K) : {
        *(.multiboot2)
        *(.text*)
    }

    .rodata ALIGN(4K) : {
        *(.rodata*)
    }

    .data ALIGN(4K) : {
        *(.data*)
        *(.bss*)
    }

    /DISCARD/ : {
        *(.comment)
    }
}

