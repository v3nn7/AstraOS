#pragma once

#include "types.h"

void apic_mock_set_bases(volatile uint32_t *lapic_base, volatile uint32_t *ioapic_base);
void lapic_init(void);
void lapic_eoi(void);
void lapic_timer_init(uint32_t divider, uint32_t initial_count);
void lapic_timer_calibrate(void);
void lapic_timer_start_1ms(void);
void ioapic_init(void);
void ioapic_redirect_irq(uint8_t irq, uint8_t vector);

