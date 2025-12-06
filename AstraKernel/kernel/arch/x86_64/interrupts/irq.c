#include "types.h"
#include "interrupts.h"

static irq_handler_t irq_handlers[16] = {0};

static void pic_remap(void) {
    outb(0x20, 0x11);
    outb(0xA0, 0x11);
    outb(0x21, 0x20);
    outb(0xA1, 0x28);
    outb(0x21, 0x04);
    outb(0xA1, 0x02);
    outb(0x21, 0x01);
    outb(0xA1, 0x01);
    outb(0x21, 0x0);
    outb(0xA1, 0x0);
}

static void pic_eoi(uint8_t irq) {
    if (irq >= 8) outb(0xA0, 0x20);
    outb(0x20, 0x20);
}

void irq_register_handler(uint8_t irq, irq_handler_t handler) {
    if (irq < 16) irq_handlers[irq] = handler;
}

void irq_init(void) {
    pic_remap();
}

static void irq_dispatch(uint8_t irq, interrupt_frame_t *frame) {
    if (irq < 16 && irq_handlers[irq]) {
        irq_handlers[irq](frame);
    }
    pic_eoi(irq);
}

#define IRQ_STUB(n) \
__attribute__((interrupt)) void irq##n##_stub(interrupt_frame_t *frame) { irq_dispatch(n, frame); }

IRQ_STUB(0)  IRQ_STUB(1)  IRQ_STUB(2)  IRQ_STUB(3)
IRQ_STUB(4)  IRQ_STUB(5)  IRQ_STUB(6)  IRQ_STUB(7)
IRQ_STUB(8)  IRQ_STUB(9)  IRQ_STUB(10) IRQ_STUB(11)
IRQ_STUB(12) IRQ_STUB(13) IRQ_STUB(14) IRQ_STUB(15)

