[bits 64]
default rel

extern kmain

global _start
global initial_stack_top

section .bss
align 16
initial_stack:
    resb 16384
initial_stack_top:

section .text
align 16
_start:
    cli
    mov rsp, initial_stack_top
    xor rbp, rbp
    call kmain
.halt:
    hlt
    jmp .halt
