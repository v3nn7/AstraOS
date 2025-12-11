.intel_syntax noprefix
.code64

.globl load_gdt
load_gdt:
    # ms_abi: rcx=descriptor (we keep current CS/SS from firmware)
    lgdt [rcx]
    ret

