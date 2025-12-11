/**
 * Prosty szkielet schedulera: globalny tick i stub sleep.
 */

#include "sched.h"
#include <drivers/timers/hpet.hpp>
extern "C" {
#include <drivers/apic/apic_timer.h>
}
#include "klog.h"

static volatile uint64_t g_ticks = 0;

extern "C" void sched_init(void) {
    g_ticks = 0;
    klog_printf(KLOG_INFO, "sched: init (stub)");
    /* Prefer LAPIC timer if calibrated; vector placeholder 0xEE masked elsewhere. */
    apic_timer_init(0xEE);
}

extern "C" void sched_tick(void) {
    g_ticks++;
}

extern "C" void sched_sleep_ms(uint64_t ms) {
    if (HPET::is_available()) {
        HPET::sleep_ms(ms);
    } else {
        apic_timer_sleep_ms((uint32_t)ms);
    }
}

extern "C" void sched_yield(void) {
    /* Stub: no ready queue yet. */
    __asm__ __volatile__("pause");
}

