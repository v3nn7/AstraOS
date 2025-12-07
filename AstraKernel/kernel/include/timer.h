#pragma once

#include "types.h"
#include "interrupts.h"

typedef void (*timer_callback_t)(uint64_t tick, void *user);

void timer_init(uint32_t hz);
uint64_t timer_ticks(void);
void timer_register_callback(timer_callback_t cb, void *user);
void timer_handle_irq(interrupt_frame_t *frame);

/* Helper for LAPIC calibration (busy wait ~10ms via PIT channel 2) */
void pit_wait_10ms(void);
void pit_wait_ms(uint32_t ms);

