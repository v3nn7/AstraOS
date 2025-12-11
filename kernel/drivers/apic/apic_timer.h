#pragma once
#include <stdint.h>

void apic_timer_init(uint32_t vector);
void apic_timer_calibrate();
void apic_timer_sleep_ms(uint32_t ms);
