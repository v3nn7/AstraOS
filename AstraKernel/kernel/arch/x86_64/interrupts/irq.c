#include "types.h"
#include "interrupts.h"
#include "apic.h"

static irq_handler_t irq_handlers[256] = {0};

void irq_register(uint8_t vector, irq_handler_t handler) {
    irq_handlers[vector] = handler;
}

/* Public API for drivers: legacy IRQ number (0-based, PIC style) */
void irq_register_handler(uint8_t irq, irq_handler_t handler) {
    /* Map legacy IRQ 0-15 to vectors 32-47 */
    uint8_t vector = (irq < 16) ? (uint8_t)(32 + irq) : irq;
    irq_register(vector, handler);
}

static void irq_dispatch(uint8_t vector, interrupt_frame_t *frame) {
    if (irq_handlers[vector])
        irq_handlers[vector](frame);

    lapic_eoi();   // KONIECZNIE — tylko LAPIC dostaje EOI
}

#define IRQ_STUB(n) \
__attribute__((interrupt)) void irq##n(interrupt_frame_t *f) { irq_dispatch(n, f); }

IRQ_STUB(32) IRQ_STUB(33) IRQ_STUB(34) IRQ_STUB(35)
IRQ_STUB(36) IRQ_STUB(37) IRQ_STUB(38) IRQ_STUB(39)
IRQ_STUB(40) IRQ_STUB(41) IRQ_STUB(42) IRQ_STUB(43)
IRQ_STUB(44) IRQ_STUB(45) IRQ_STUB(46) IRQ_STUB(47)

static void pic_disable(void) {
    outb(0x21, 0xFF);
    outb(0xA1, 0xFF);
    outb(0x22, 0x70);
    outb(0x23, 0x01);
}

void irq_init(void) {
    pic_disable();
    lapic_init();
    ioapic_init();

    // Timer → 32
    ioapic_redirect_irq(0, 32);
    // Keyboard → 33
    ioapic_redirect_irq(1, 33);
}
