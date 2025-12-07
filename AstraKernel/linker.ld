/* -------------------------------------------
   AstraOS linker script (Limine 10.x ready)
   Higher-half kernel: 0xFFFFFFFF80000000
   Physical load:      0x00100000
------------------------------------------- */

ENTRY(_start)

KERNEL_VMA = 0xFFFFFFFF80000000;
KERNEL_LMA = 0x00100000;

SECTIONS
{
    /* Load at 1MiB physical, map to higher half */
    . = KERNEL_LMA;
    
    __kernel_phys_start = .;
    
    /* LIMINE HEADER (MUST BE AT BEGINNING!) */
    .limine_hdr : AT(ADDR(.limine_hdr))
    {
        KEEP(*(.limine_hdr))
    }
    
    . += KERNEL_VMA;
    
    __kernel_virt_start = .;
    
    /* Stivale2 header must be kept and placed early */
    .stivale2hdr ALIGN(16) : AT(ADDR(.stivale2hdr) - KERNEL_VMA)
    {
        KEEP(*(.stivale2hdr))
    }
    
    /* Text section */
    .text ALIGN(4K) : AT(ADDR(.text) - KERNEL_VMA)
    {
        *(.text.start)
        *(.text._start)
        *(.text*)
        *(.rodata*)
    }
    
    /* Initcall descriptors */
    .initcalls ALIGN(8) : AT(ADDR(.initcalls) - KERNEL_VMA)
    {
        __start_initcalls = .;
        KEEP(*(.initcalls))
        __stop_initcalls = .;
    }
    
    /* Data section */
    .data ALIGN(4K) : AT(ADDR(.data) - KERNEL_VMA)
    {
        *(.data*)
        *(.sdata*)
    }
    
    /* BSS section */
    .bss ALIGN(4K) : AT(ADDR(.bss) - KERNEL_VMA)
    {
        *(COMMON)
        *(.bss*)
        *(.sbss*)
    }
    
    __kernel_virt_end = .;
    __kernel_phys_end = . - KERNEL_VMA;
    /* Public kernel bounds for C code */
    _kernel_start = __kernel_virt_start;
    _kernel_end   = __kernel_virt_end;
    
    /* Discard unnecessary sections */
    /DISCARD/ :
    {
        *(.eh_frame*)
        *(.comment*)
        *(.note*)
    }
}