; AstraOS - ISR Common Stub (Assembly)
; Common interrupt handler stub that can be used for all interrupts
; Calls C function interrupt_handler(vector, frame)

section .text

; External C function
extern interrupt_handler

; Common ISR stub - saves all registers and calls C handler
; This is a generic stub that can be used for any interrupt vector
; Parameters:
;   - Vector number is passed as immediate value or in register
;   - Frame pointer is on stack (from interrupt)
isr_common:
    ; Save all general-purpose registers
    push rax
    push rbx
    push rcx
    push rdx
    push rsi
    push rdi
    push rbp
    push r8
    push r9
    push r10
    push r11
    push r12
    push r13
    push r14
    push r15
    
    ; Save segment registers (as 64-bit values)
    mov rax, ds
    push rax
    mov rax, es
    push rax
    mov rax, fs
    push rax
    mov rax, gs
    push rax
    
    ; Set kernel data segments
    mov ax, 0x10  ; Kernel data segment selector
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    
    ; Call C handler
    ; interrupt_handler(vector, frame)
    ; Frame is at RSP + (15*8) = RSP + 120 (after all pushes)
    ; Vector should be passed in RDI (first argument)
    mov rsi, rsp  ; Frame pointer (second argument)
    ; RDI should already contain vector number
    call interrupt_handler
    
    ; Restore segment registers
    pop rax
    mov gs, ax
    pop rax
    mov fs, ax
    pop rax
    mov es, ax
    pop rax
    mov ds, ax
    
    ; Restore general-purpose registers
    pop r15
    pop r14
    pop r13
    pop r12
    pop r11
    pop r10
    pop r9
    pop r8
    pop rbp
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop rbx
    pop rax
    
    ; Return from interrupt
    iretq

; Macro to create ISR stub for specific vector
%macro ISR_STUB_WITH_VECTOR 1
global isr%1_stub
isr%1_stub:
    ; Push vector number (for error code interrupts, this is already on stack)
    push %1
    ; Jump to common handler
    jmp isr_common
%endmacro

; Macro to create ISR stub without error code
%macro ISR_STUB_NOERR 1
global isr%1_stub
isr%1_stub:
    push 0        ; Dummy error code
    push %1       ; Vector number
    jmp isr_common
%endmacro

; Macro to create ISR stub with error code
%macro ISR_STUB_ERR 1
global isr%1_stub
isr%1_stub:
    ; Error code is already pushed by CPU
    push %1       ; Vector number
    jmp isr_common
%endmacro
