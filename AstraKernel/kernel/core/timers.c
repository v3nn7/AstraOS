#include "timer.h"
#include "interrupts.h"
#include "kernel.h"
#include "klog.h"
#include "apic.h"

#define PIT_INPUT_HZ 1193182
#define MAX_TIMER_CALLBACKS 8

typedef struct {
    timer_callback_t cb;
    void *user;
} timer_cb_entry_t;

static timer_cb_entry_t callbacks[MAX_TIMER_CALLBACKS];
static uint64_t tick_counter = 0;

void pit_wait_10ms(void) {
    /* Use PIT channel 2 one-shot for ~10ms */
    uint16_t divisor = (uint16_t)(PIT_INPUT_HZ / 100); /* 10ms => 100 Hz */

    /* Enable gate, disable speaker output */
    uint8_t val61 = inb(0x61);
    val61 = (val61 & ~0x02) | 0x01; /* gate=1, speaker off */
    outb(0x61, val61);

    /* Program channel 2, mode 0, lobyte/hibyte */
    outb(0x43, 0xB0);
    outb(0x42, (uint8_t)(divisor & 0xFF));
    outb(0x42, (uint8_t)(divisor >> 8));

    /* Wait until OUT goes high (bit5 in port 0x61) */
    while (!(inb(0x61) & 0x20)) { }
}

void pit_wait_ms(uint32_t ms) {
    if (ms == 0) return;
    uint32_t chunks = (ms + 9) / 10; /* use 10ms granularity */
    for (uint32_t i = 0; i < chunks; ++i) {
        pit_wait_10ms();
    }
}

void timer_register_callback(timer_callback_t cb, void *user) {
    for (int i = 0; i < MAX_TIMER_CALLBACKS; ++i) {
        if (!callbacks[i].cb) {
            callbacks[i].cb = cb;
            callbacks[i].user = user;
            return;
        }
    }
}

uint64_t timer_ticks(void) { return tick_counter; }

void timer_handle_irq(interrupt_frame_t *frame) {
    ++tick_counter;
    for (int i = 0; i < MAX_TIMER_CALLBACKS; ++i) {
        if (callbacks[i].cb) callbacks[i].cb(tick_counter, callbacks[i].user);
    }
    scheduler_tick(frame);
}

void timer_init(uint32_t hz) {
    if (hz == 0) hz = 250;
    uint16_t divisor = (uint16_t)(PIT_INPUT_HZ / hz);

    outb(0x43, 0x36);
    outb(0x40, (uint8_t)(divisor & 0xFF));
    outb(0x40, (uint8_t)((divisor >> 8) & 0xFF));

    klog_printf(KLOG_INFO, "Timer: PIT %d Hz (div=%d)", (int)hz, (int)divisor);
    irq_register_handler(0, timer_handle_irq);

    /* LAPIC timer ~1 kHz (calibrated using PIT wait) */
    lapic_timer_start_1ms();

    /* Mask legacy PIC timer to avoid double IRQ0 when LAPIC is active */
    uint8_t mask = inb(0x21);
    mask |= 0x01;
    outb(0x21, mask);
}
