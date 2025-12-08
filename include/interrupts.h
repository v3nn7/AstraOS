#pragma once

#include "types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct interrupt_frame {
    uint64_t rip;
    uint64_t cs;
    uint64_t rflags;
    uint64_t rsp;
    uint64_t ss;
} interrupt_frame_t;

typedef void (*irq_handler_t)(interrupt_frame_t *frame);

static inline void interrupts_enable(void) {}
static inline void interrupts_disable(void) {}

void irq_register(uint8_t vector, irq_handler_t handler);
void irq_eoi(uint8_t vector);

#ifdef __cplusplus
}
#endif

