.intel_syntax noprefix
.code64

.text

.globl isr_stub_default
isr_stub_default:
    cli
    iretq
