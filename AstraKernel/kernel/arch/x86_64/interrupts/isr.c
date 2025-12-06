#include "types.h"
#include "interrupts.h"
#include "kernel.h"
#include "vmm.h"

static const char *messages[32] = {
    "Divide-by-zero", "Debug", "NMI", "Breakpoint", "Overflow", "BOUND",
    "Invalid opcode", "Device not available", "Double fault", "Coprocessor segment overrun",
    "Invalid TSS", "Segment not present", "Stack fault", "General protection fault", "Page fault",
    "Reserved", "x87 floating point", "Alignment check", "Machine check", "SIMD FP",
    "Virtualization", "Control protection", "Reserved", "Reserved", "Reserved",
    "Reserved", "Reserved", "Reserved", "Reserved", "Reserved", "Security exception", "Triple fault"
};

static void panic_exception(const char *name, uint64_t code, interrupt_frame_t *frame) {
    printf("EXC: %s err=%x rip=%p rsp=%p cr2=%p\n", name, code, (void *)frame->rip, (void *)frame->rsp, (void *)read_cr2());
    while (1) { __asm__ volatile("cli; hlt"); }
}

#define DEFINE_ISR_NOERR(num) \
__attribute__((interrupt)) void isr##num(interrupt_frame_t *frame) { \
    panic_exception(messages[num], 0, frame); \
}

#define DEFINE_ISR_ERR(num) \
__attribute__((interrupt)) void isr##num(interrupt_frame_t *frame, uint64_t code) { \
    panic_exception(messages[num], code, frame); \
}

DEFINE_ISR_NOERR(0)  DEFINE_ISR_NOERR(1)  DEFINE_ISR_NOERR(2)  DEFINE_ISR_NOERR(3)
DEFINE_ISR_NOERR(4)  DEFINE_ISR_NOERR(5)  DEFINE_ISR_NOERR(6)  DEFINE_ISR_NOERR(7)
DEFINE_ISR_ERR(8)    DEFINE_ISR_NOERR(9)  DEFINE_ISR_ERR(10)   DEFINE_ISR_ERR(11)
DEFINE_ISR_ERR(12)   DEFINE_ISR_ERR(13)   DEFINE_ISR_NOERR(15)
DEFINE_ISR_NOERR(16) DEFINE_ISR_ERR(17)   DEFINE_ISR_NOERR(18) DEFINE_ISR_NOERR(19)
DEFINE_ISR_NOERR(20) DEFINE_ISR_ERR(30)

__attribute__((interrupt)) void isr14(interrupt_frame_t *frame, uint64_t code) {
    vmm_page_fault_handler(frame, code);
}

