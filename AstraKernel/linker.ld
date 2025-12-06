ENTRY(_start)

KERNEL_VMA = 0xFFFFFFFF80000000;
KERNEL_LMA = 0x00100000;

SECTIONS
{
    . = KERNEL_VMA + KERNEL_LMA;
    _kernel_start = .;
    _kernel_start_physical = KERNEL_LMA;

    .limine_reqs ALIGN(8) : AT(ADDR(.limine_reqs) - KERNEL_VMA)
    {
        KEEP(*(.limine_reqs))
    }

    .text ALIGN(0x1000) : AT(ADDR(.text) - KERNEL_VMA)
    {
        *(.text*)
        *(.rodata*)
    }

    .data ALIGN(0x1000) : AT(ADDR(.data) - KERNEL_VMA)
    {
        *(.data*)
    }

    .bss ALIGN(0x1000) : AT(ADDR(.bss) - KERNEL_VMA)
    {
        *(COMMON)
        *(.bss*)
    }

    _kernel_end = .;
    _kernel_end_physical = _kernel_end - KERNEL_VMA;

    /DISCARD/ :
    {
        *(.comment)
        *(.note*)
        *(.eh_frame*)
    }
}
