    .section .multiboot2
    .align 8
    .long 0xE85250D6          # multiboot2 magic
    .long 0                   # architecture
    .long header_end - header_start
    .long -(0xE85250D6 + 0 + (header_end - header_start))
header_start:
    .word 0                   # end tag type
    .word 0
    .long 8
header_end:

