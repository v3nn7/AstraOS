[bits 64]
default rel

extern kmain
global _start
global initial_stack_top

section .bss
align 16
stack_bottom:
    resb 16384
stack_top:
initial_stack_top:

section .text
align 16
_start:
    cli

    ; Align stack to 16 bytes (SysV ABI requires: RSP % 16 == 0 before call)
    mov     rsp, stack_top
    and     rsp, -16

    xor     rbp, rbp

    ; (optional but recommended)
    xor     rax, rax
    xor     rbx, rbx
    xor     rcx, rcx
    xor     rdx, rdx
    xor     rsi, rsi
    xor     rdi, rdi
    xor     r8,  r8
    xor     r9,  r9
    xor     r10, r10
    xor     r11, r11

    call kmain

.hang:
    hlt
    jmp .hang
