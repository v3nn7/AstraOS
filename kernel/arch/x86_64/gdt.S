.intel_syntax noprefix

.globl load_gdt
load_gdt:
    lgdt [rdi]
    mov ax, dx          # data_sel
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov fs, ax
    mov gs, ax
    pushq rsi           # code_sel
    lea rax, [rip + 1f]
    pushq rax
    lretq
1:
    ret

