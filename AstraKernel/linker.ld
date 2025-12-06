/* -------------------------------------------
   AstraOS linker script (Limine 10.x ready)
   Higher-half kernel: 0xFFFFFFFF80000000
   Physical load:      0x00100000
------------------------------------------- */

ENTRY(_start)

/* Virtual and physical mappings */
KERNEL_VMA = 0xFFFFFFFF80000000;
KERNEL_LMA = 0x00100000;

SECTIONS
{
    /* Load address = physical + virtual offset */
    . = KERNEL_VMA + KERNEL_LMA;

    _kernel_start           = .;
    _kernel_start_physical  = KERNEL_LMA;

    /* -------------------------------------------
       LIMINE REQUESTS (start, body, end)
    ------------------------------------------- */
    .limine_requests ALIGN(8) : AT(ADDR(.limine_requests) - KERNEL_VMA)
    {
        KEEP(*(.limine_requests_start))
        KEEP(*(.limine_requests))
        KEEP(*(.limine_requests_end))
    }

    /* -------------------------------------------
       TEXT SECTION
    ------------------------------------------- */
    .text ALIGN(0x1000) : AT(ADDR(.text) - KERNEL_VMA)
    {
        KEEP(*(.text.start))
        KEEP(*(.text._start))
        KEEP(*(.init))
        *(.text*)
        *(.rodata*)
    }

    /* -------------------------------------------
       DATA SECTION
    ------------------------------------------- */
    .data ALIGN(0x1000) : AT(ADDR(.data) - KERNEL_VMA)
    {
        *(.data*)
        *(.sdata*)
    }

    /* -------------------------------------------
       BSS SECTION
    ------------------------------------------- */
    .bss ALIGN(0x1000) : AT(ADDR(.bss) - KERNEL_VMA)
    {
        *(COMMON)
        *(.bss*)
        *(.sbss*)
    }

    /* Mark kernel end */
    _kernel_end          = .;
    _kernel_end_physical = _kernel_end - KERNEL_VMA;

    /* -------------------------------------------
       REMOVE USELESS SECTIONS
    ------------------------------------------- */
    /DISCARD/ :
    {
        *(.eh_frame*)
        *(.comment*)
        *(.note*)
        *(.note.*)
    }
}
