; Simple syscall trampoline returning ENOSYS (-38).
; This is a placeholder until a full syscall table is wired.
; Assumes this entry is hooked from IDT (e.g., vector 0x80).

global syscall_entry

section .text
align 16

syscall_entry:
    mov rax, -38          ; ENOSYS
    iretq
