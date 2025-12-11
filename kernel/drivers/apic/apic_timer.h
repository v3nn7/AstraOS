#pragma once
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void apic_timer_init(uint32_t vector);
void apic_timer_calibrate(void);
void apic_timer_sleep_ms(uint32_t ms);

#ifdef __cplusplus
}
#endif
