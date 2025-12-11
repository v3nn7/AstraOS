.intel_syntax noprefix

.globl load_idt
load_idt:
    lidt [rdi]
    ret

