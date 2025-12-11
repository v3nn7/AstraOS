/**
 * Prosty szkielet schedulera: globalny tick i stub sleep.
 */

#include "sched.h"
#include "drivers/timers/hpet.hpp"
#include "apic_timer.h"
#include "klog.h"

static volatile uint64_t g_ticks = 0;

void sched_init(void) {
    g_ticks = 0;
    klog_printf(KLOG_INFO, "sched: init (stub)");
    /* Prefer LAPIC timer if calibrated; otherwise HPET sleep path remains. */
    apic_timer_init(0xEE); /* masked LVT setup elsewhere; vector placeholder */
}

void sched_tick(void) {
    g_ticks++;
}

void sched_sleep_ms(uint64_t ms) {
    if (HPET::is_available()) {
        HPET::sleep_ms(ms);
    } else {
        apic_timer_sleep_ms((uint32_t)ms);
    }
}

void sched_yield(void) {
    /* Stub: no ready queue yet. */
    __asm__ __volatile__("pause");
}

