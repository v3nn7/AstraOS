.intel_syntax noprefix
.code64

.globl load_idt
load_idt:
    # ms_abi: rcx = idt descriptor
    lidt [rcx]
    ret

