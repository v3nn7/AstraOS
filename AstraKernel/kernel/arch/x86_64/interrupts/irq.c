#include "types.h"
#include "interrupts.h"
#include "apic.h"
#include "klog.h"
#include "kernel.h"

static irq_handler_t irq_handlers[256] = {0};

void irq_register(uint8_t vector, irq_handler_t handler) {
    printf("irq_register: vector=%u handler=%p (before: %p)\n", vector, handler, irq_handlers[vector]);
    irq_handlers[vector] = handler;
    printf("irq_register: vector=%u handler=%p (after: %p)\n", vector, handler, irq_handlers[vector]);
}

/* Public API for drivers: legacy IRQ number (0-based, PIC style) */
void irq_register_handler(uint8_t irq, irq_handler_t handler) {
    /* Map legacy IRQ 0-15 to vectors 32-47 */
    uint8_t vector = (irq < 16) ? (uint8_t)(32 + irq) : irq;
    printf("irq: registering IRQ%u -> vector %u (0x%02X) handler=%p\n", irq, vector, vector, handler);
    irq_register(vector, handler);
    if (irq == 12) {
        printf("mouse: IRQ registered (IRQ12 -> vector 44)\n");
    }
}

/* Common interrupt handler - can be called from assembly stubs */
void interrupt_handler(uint8_t vector, interrupt_frame_t *frame) {
    /* DIAGNOSTIC: Print critical interrupts */
    if (vector == 44) {
        printf("MOUSE IRQ FIRED! (vector 44, IRQ12) handler=%p\n", irq_handlers[vector]);
    }
    
    if (irq_handlers[vector]) {
        irq_handlers[vector](frame);
    } else {
        if (vector >= 32 && vector <= 47) {
            printf("INT VECTOR = %u (0x%02X) - no handler registered\n", vector, vector);
        }
    }

    lapic_eoi();   // KONIECZNIE — tylko LAPIC dostaje EOI
}

/* Internal dispatch function */
static void irq_dispatch(uint8_t vector, interrupt_frame_t *frame) {
    interrupt_handler(vector, frame);
}

/* IRQ stub macro - creates interrupt handler for specific vector */
/* These stubs are called directly from IDT when interrupt fires */
#define IRQ_STUB(n) \
__attribute__((interrupt)) __attribute__((visibility("default"))) void irq##n(interrupt_frame_t *f) { \
    interrupt_handler(n, f); \
}

IRQ_STUB(32) IRQ_STUB(33) IRQ_STUB(34) IRQ_STUB(35)
IRQ_STUB(36) IRQ_STUB(37) IRQ_STUB(38) IRQ_STUB(39)
IRQ_STUB(40) IRQ_STUB(41) IRQ_STUB(42) IRQ_STUB(43)
IRQ_STUB(44) /* Mouse IRQ12 -> vector 44 (0x2C) */
IRQ_STUB(45) IRQ_STUB(46) IRQ_STUB(47)

static void pic_disable(void) {
    printf("irq: disabling PIC (masking all IRQs)\n");
    /* Mask all PIC interrupts */
    outb(0x21, 0xFF); /* Master PIC - mask all IRQs 0-7 */
    outb(0xA1, 0xFF); /* Slave PIC - mask all IRQs 8-15 (including IRQ12) */
    
    /* Disable PIC by remapping to unused vectors */
    outb(0x20, 0x11); /* ICW1: initialize master PIC */
    outb(0xA0, 0x11); /* ICW1: initialize slave PIC */
    outb(0x21, 0x70); /* ICW2: master PIC base = 0x70 (unused) */
    outb(0xA1, 0x78); /* ICW2: slave PIC base = 0x78 (unused) */
    outb(0x21, 0x04); /* ICW3: master has slave on IRQ2 */
    outb(0xA1, 0x02); /* ICW3: slave ID = 2 */
    outb(0x21, 0x01); /* ICW4: master PIC mode */
    outb(0xA1, 0x01); /* ICW4: slave PIC mode */
    
    /* Mask all interrupts again */
    outb(0x21, 0xFF);
    outb(0xA1, 0xFF);
    
    printf("irq: PIC disabled and masked\n");
}

void irq_init(void) {
    pic_disable();
    lapic_init();
    ioapic_init();

    // Timer → 32
    ioapic_redirect_irq(0, 32);
    // Keyboard → 33
    ioapic_redirect_irq(1, 33);
    // Mouse → 44 (IRQ12 maps to vector 44 = 32 + 12)
    ioapic_redirect_irq(12, 44);
    
    klog_printf(KLOG_INFO, "irq: initialized - timer(0->32), keyboard(1->33), mouse(12->44)");
}
