#pragma once
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define LAPIC_ID        0x020
#define LAPIC_VERSION   0x030
#define LAPIC_TPR       0x080
#define LAPIC_EOI       0x0B0
#define LAPIC_SVR       0x0F0
#define LAPIC_ESR       0x280
#define LAPIC_ICR_LOW   0x300
#define LAPIC_ICR_HIGH  0x310
#define LAPIC_LVT_TIMER 0x320
#define LAPIC_LVT_LINT0 0x350
#define LAPIC_LVT_LINT1 0x360
#define LAPIC_INIT_COUNT 0x380
#define LAPIC_CURR_COUNT 0x390
#define LAPIC_DIVIDE    0x3E0

#ifdef __cplusplus
extern "C" {
#endif

void lapic_init();
void lapic_eoi();
void lapic_write(uint32_t reg, uint32_t value);
uint32_t lapic_read(uint32_t reg);
void lapic_send_ipi(uint32_t apic_id, uint32_t flags);

#ifdef __cplusplus
}
#endif

#ifdef __cplusplus
}
#endif
