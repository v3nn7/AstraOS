#include <interrupts.h>

void irq_register(uint8_t vector, irq_handler_t handler) {
    (void)vector;
    (void)handler;
}

void irq_eoi(uint8_t vector) {
    (void)vector;
}

