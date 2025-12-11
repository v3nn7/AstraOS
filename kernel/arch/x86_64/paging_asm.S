.intel_syntax noprefix
.code64

.globl read_cr0
read_cr0:
    mov rax, cr0
    ret

.globl write_cr0
write_cr0:
    # rcx holds value (ms_abi)
    mov cr0, rcx
    ret

.globl read_cr4
read_cr4:
    mov rax, cr4
    ret

.globl write_cr4
write_cr4:
    # rcx holds value (ms_abi)
    mov cr4, rcx
    ret

.globl load_cr3
load_cr3:
    # rcx holds new CR3 (ms_abi)
    mov cr3, rcx
    ret
