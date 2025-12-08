/* =============================
   MULTIBOOT2 HEADER (GRUB)
   ============================= */

    .section .multiboot_header, "a"
    .align 8

multiboot_header_start:
    /* Multiboot2 magic */
    .long 0xE85250D6          /* magic */
    .long 0                   /* architecture = 0 (i386) */

    /* Header length */
    .long multiboot_header_end - multiboot_header_start

    /* Checksum = -(magic + arch + length) */
    .long -(0xE85250D6 + 0 + (multiboot_header_end - multiboot_header_start))

    /* =============================
       FRAMEBUFFER REQUEST (tag type 5)
       ============================= */
    .align 8
    .word 5                   /* tag type = framebuffer */
    .word 0                   /* flags */
    .long 20                  /* tag size */
    .long 1024                /* width */
    .long 768                 /* height */
    .long 32                  /* bpp */

    /* =============================
       END TAG
       ============================= */
    .align 8
    .word 0                   /* type = 0 */
    .word 0                   /* flags = 0 */
    .long 8                   /* size = 8 bytes */

multiboot_header_end:
