#include "apic_timer.h"
#include "lapic.h"

static uint32_t bus_freq = 0;

void apic_timer_calibrate() {
    lapic_write(LAPIC_DIVIDE, 0x3);      // divide by 16
    lapic_write(LAPIC_INIT_COUNT, 0xFFFFFFFF);

    // Wait 10ms using PIT or HPET
    extern void sleep_10ms();
    sleep_10ms();

    uint32_t delta = 0xFFFFFFFF - lapic_read(LAPIC_CURR_COUNT);
    bus_freq = delta * 100; // ticks per second
}

void apic_timer_init(uint32_t vector) {
    lapic_write(LAPIC_LVT_TIMER, vector);
    lapic_write(LAPIC_DIVIDE, 0x3); // /16
    lapic_write(LAPIC_INIT_COUNT, bus_freq / 100); // 10ms tick
}

void apic_timer_sleep_ms(uint32_t ms) {
    for (uint32_t i = 0; i < ms; i++) {
        lapic_write(LAPIC_INIT_COUNT, bus_freq / 1000);
        while (lapic_read(LAPIC_CURR_COUNT) != 0) { asm volatile("pause"); }
    }
}
