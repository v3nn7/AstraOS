#include "types.h"
#include "interrupts.h"
#include "kernel.h"
#include "vmm.h"

static const char *exc_name[32] = {
    "Divide-by-zero", "Debug", "NMI", "Breakpoint", "Overflow", "BOUND",
    "Invalid opcode", "Device not available", "Double fault",
    "Coprocessor segment overrun",
    "Invalid TSS", "Segment not present", "Stack fault",
    "General protection", "Page fault",
    "Reserved",
    "x87 FP", "Alignment check", "Machine check", "SIMD FP",
    "Virtualization", "Control protection", "Reserved", "Reserved",
    "Reserved", "Reserved", "Reserved", "Reserved", "Reserved",
    "Reserved", "Security exception"
};

static void panic_exception(const char *msg, uint64_t err, interrupt_frame_t *f) {
    uint64_t cr2 = read_cr2();
    /* printf supports %x and %p; use 32-bit for error code */
    printf("[EXC] %s err=%x RIP=%p RSP=%p CR2=%p\n",
           msg, (uint32_t)err, (void*)f->rip, (void*)f->rsp, (void*)cr2);
    while (1) __asm__("cli; hlt");
}

#define ISR_NOERR(n) \
__attribute__((interrupt)) void isr##n(interrupt_frame_t *f) { \
    panic_exception(exc_name[n], 0, f); }

#define ISR_ERR(n) \
__attribute__((interrupt)) void isr##n(interrupt_frame_t *f, uint64_t err) { \
    panic_exception(exc_name[n], err, f); }

ISR_NOERR(0) ISR_NOERR(1) ISR_NOERR(2) ISR_NOERR(3)
ISR_NOERR(4) ISR_NOERR(5) ISR_NOERR(6) ISR_NOERR(7)

ISR_ERR(8)  ISR_NOERR(9)  ISR_ERR(10) ISR_ERR(11)
ISR_ERR(12) ISR_ERR(13) ISR_NOERR(15)
ISR_NOERR(16) ISR_ERR(17) ISR_NOERR(18) ISR_NOERR(19)
ISR_NOERR(20) ISR_ERR(30)

// Page Fault
__attribute__((interrupt))
void isr14(interrupt_frame_t *f, uint64_t err) {
    vmm_page_fault_handler(f, err);
}
