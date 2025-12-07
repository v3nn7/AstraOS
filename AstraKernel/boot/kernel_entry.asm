; ============================================================
;  kernel_entry.asm — poprawny Stivale2 entrypoint dla Limine 10.x
; ============================================================

BITS 64
default rel

global _start
global initial_stack_top
extern kmain

; ============================================================
; 1. STACK SECTION
; ============================================================
section .bss
align 16
stack_bottom:
    resb 16384                  ; 16 KB stack
stack_top:
initial_stack_top:

; ============================================================
; 2. STIVALE2 FRAMEBUFFER TAG
; ============================================================
section .stivale2hdr
align 16

fb_tag:
    dq 0x3ecc1bc43d0f7971       ; Stivale2 framebuffer tag ID
    dq 0                        ; next tag = end of list
    dd 1920                     ; width
    dd 1080                     ; height
    dd 32                       ; bpp
    dd 0                        ; reserved/padding

; ============================================================
; 3. STIVALE2 HEADER
; ============================================================
global stivale2_header
stivale2_header:
    dq 0                        ; entry_point = 0 → Limine jumps to _start
    dq stack_top                ; where to set stack pointer
    dq 0                        ; flags
    dq fb_tag                   ; first tag pointer

; ============================================================
; 4. KERNEL ENTRY
; ============================================================
section .text
align 16

_start:
    cli

    ; set stack (Limine sets its own but we override)
    mov     rsp, stack_top
    and     rsp, -16

    xor     rbp, rbp

    ; clear registers to avoid UB in early kernel
    xor rax, rax
    xor rbx, rbx
    xor rcx, rcx
    xor rdx, rdx
    xor rsi, rsi
    xor rdi, rdi
    xor r8,  r8
    xor r9,  r9
    xor r10, r10
    xor r11, r11
    xor r12, r12
    xor r13, r13
    xor r14, r14
    xor r15, r15

    ; ========================================================
    ; CALL MAIN KERNEL FUNCTION
    ; Limine passes pointer to stivale2 struct → rdi
    ; ========================================================
    mov rdi, rsi        ; rsi = pointer to stivale2 struct
    call kmain

.hang:
    hlt
    jmp .hang
