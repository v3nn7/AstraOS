#pragma once
#include <stdint.h>

void ioapic_init();
void ioapic_write(uint8_t reg, uint32_t val);
uint32_t ioapic_read(uint8_t reg);
void ioapic_redirect_irq(uint8_t irq, uint32_t apic_id, uint8_t vector);
uint32_t ioapic_get_gsi_base();
