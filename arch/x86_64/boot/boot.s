    .section .multiboot2
    .align 8
multiboot2_header_start:
    .long 0xE85250D6          # multiboot2 magic
    .long 0                   # architecture
    .long multiboot2_header_end - multiboot2_header_start
    .long -(0xE85250D6 + 0 + (multiboot2_header_end - multiboot2_header_start))
    .word 0                   # end tag type
    .word 0
    .long 8
multiboot2_header_end:

    .section .text
    .code32
    .global _start
_start:
    cli
    /* TODO: transition to long mode (setup paging, enable PAE/long mode) */
    /* Placeholder jump to kernel main */
    call kmain
.hang:
    hlt
    jmp .hang

