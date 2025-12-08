/* ==========================================================
   PAGE TABLES, STACK, TEMP VARIABLES
   ========================================================== */

.section .bss
    .align 4096
pml4_table:     .skip 4096
pdpt_table:     .skip 4096
pd_tables:      .skip 4096 * 4      /* 4x PD → 4 GiB to cover framebuffer */

    .align 16
stack64:        .skip 16384
stack64_top:

mb_info_tmp:
    .long 0


/* ==========================================================
   ENTRY POINT (EXECUTED BY GRUB)
   ========================================================== */

.section .text._start
.global _start
.code32
_start:
    cli

    /* save multiboot2 info pointer */
    mov %ebx, mb_info_tmp


/* ==========================================================
   CLEAR PAGE TABLES
   ========================================================== */
    xor %eax, %eax
    mov $(1024 * (1 + 1 + 4)), %ecx  /* pml4 + pdpt + 4xPD */
    mov $pml4_table, %edi
    rep stosl


/* ==========================================================
   SETUP 4GB IDENTITY MAPPING USING 2MB PAGES
   ========================================================== */

    mov $pd_tables, %esi             /* current PD base */
    xor %ebx, %ebx                   /* PDPT index */
    xor %eax, %eax                   /* physical address cursor */
pd_tables_fill:
        mov %esi, %edi
        mov $512, %ecx
1:
            mov %eax, %edx
            or $0x83, %edx           /* PRESENT | RW | PS */
            mov %edx, (%edi)
            add $8, %edi
            add $0x200000, %eax
            loop 1b

        /* link PD → PDPT */
        mov %esi, %edx
        or $0x3, %edx
        mov %edx, pdpt_table(,%ebx,8)

        add $4096, %esi              /* next PD */
        inc %ebx
        cmp $4, %ebx
        jb pd_tables_fill

    /* link PDPT → PML4 */
    mov $pdpt_table, %eax
    or $0x3, %eax
    mov %eax, pml4_table


/* ==========================================================
   ENABLE PAE + LOAD PML4
   ========================================================== */

    mov %cr4, %eax
    or $0x20, %eax        /* CR4.PAE */
    mov %eax, %cr4

    mov $pml4_table, %eax
    mov %eax, %cr3


/* ==========================================================
   ENABLE LONG MODE (IA32_EFER.LME)
   ========================================================== */

    mov $0xC0000080, %ecx
    rdmsr
    or $0x100, %eax       /* LME bit */
    wrmsr


/* ==========================================================
   ENABLE PAGING IN CR0
   ========================================================== */

    mov %cr0, %eax
    or $0x80000001, %eax  /* PG | PE */
    mov %eax, %cr0


/* ==========================================================
   BUILD PROPER GDT DESCRIPTOR (PHYSICAL) AND LOAD IT
   ========================================================== */

    mov $(gdt64_end - gdt64 - 1), %ax
    mov %ax, gdt64_desc

    mov $gdt64, %eax
    mov %eax, gdt64_desc + 2

    lgdt gdt64_desc


/* ==========================================================
   JUMP TO LONG MODE 64-bit
   ========================================================== */

    ljmp $0x08, $long_mode_entry



/* ==========================================================
   64-BIT MODE
   ========================================================== */

.code64
long_mode_entry:
    mov $0x10, %ax
    mov %ax, %ds
    mov %ax, %es
    mov %ax, %ss

    /* setup stack */
    lea stack64_top(%rip), %rsp

    /* pass multiboot2 info */
    mov mb_info_tmp(%rip), %rdi

    /* call kernel */
    .extern kmain
    call kmain

.hang:
    hlt
    jmp .hang



/* ==========================================================
   64-BIT GDT (VALID FOR LONG MODE)
   ========================================================== */

.section .rodata
    .align 8

gdt64:
    .quad 0x0000000000000000          /* NULL */
    .quad 0x00AF9A000000FFFF          /* 64-bit CODE */
    .quad 0x00AF92000000FFFF          /* 64-bit DATA */

gdt64_desc:
    .word 0
    .long 0

gdt64_end:
