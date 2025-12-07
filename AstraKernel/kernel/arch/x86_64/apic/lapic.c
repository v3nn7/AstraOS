#include "apic.h"
#include "mmio.h"
#include "memory.h"
#include "klog.h"
#include "timer.h"

extern void pit_wait_10ms(void);

#define LAPIC_DEFAULT_PHYS 0xFEE00000
#define REG_LAPIC_ID       0x20
#define REG_LAPIC_EOI      0xB0
#define REG_LAPIC_SPURIOUS 0xF0
#define REG_LVT_TIMER      0x320
#define REG_TIMER_INITCNT  0x380
#define REG_TIMER_CURRCNT  0x390
#define REG_TIMER_DIVIDE   0x3E0

volatile uint32_t *apic_lapic_base = NULL;
volatile uint32_t *apic_ioapic_base = NULL; /* defined here for shared mock setter */

static inline volatile uint32_t *lapic_reg(uint32_t reg) {
    return (volatile uint32_t *)((uintptr_t)apic_lapic_base + reg);
}

void apic_mock_set_bases(volatile uint32_t *lapic_base, volatile uint32_t *ioapic_base) {
    apic_lapic_base = lapic_base;
    apic_ioapic_base = ioapic_base;
    if (!apic_lapic_base) apic_lapic_base = (volatile uint32_t *)(pmm_hhdm_offset + LAPIC_DEFAULT_PHYS);
}

static inline uint32_t lapic_read(uint32_t reg) { return mmio_read32(lapic_reg(reg)); }
static inline void lapic_write(uint32_t reg, uint32_t val) { mmio_write32(lapic_reg(reg), val); }

static uint32_t lapic_ticks_per_ms = 0;
static const uint32_t lapic_divider_code = 0x7; /* divide by 128 */

void lapic_init(void) {
    if (!apic_lapic_base) apic_mock_set_bases(NULL, apic_ioapic_base);
    
    /* Read current spurious interrupt register */
    uint32_t svr = lapic_read(REG_LAPIC_SPURIOUS);
    
    /* Enable APIC globally: bit 8 = APIC Software Enable */
    /* Set spurious vector to 0xFF (or any value >= 0x20) */
    lapic_write(REG_LAPIC_SPURIOUS, (svr & ~0xFF) | 0x100 | 0xFF);
    
    /* Verify APIC is enabled */
    uint32_t svr_after = lapic_read(REG_LAPIC_SPURIOUS);
    if (!(svr_after & 0x100)) {
        klog_printf(KLOG_WARN, "lapic: WARNING - APIC Software Enable bit not set!");
    }
    
    klog_printf(KLOG_INFO, "lapic: id=%x base=%p enabled=%d", 
                lapic_read(REG_LAPIC_ID) >> 24, apic_lapic_base, (svr_after & 0x100) != 0);
}

void lapic_eoi(void) {
    if (!apic_lapic_base) apic_mock_set_bases(NULL, apic_ioapic_base);
    lapic_write(REG_LAPIC_EOI, 0);
}

void lapic_timer_init(uint32_t divider, uint32_t initial_count) {
    if (!apic_lapic_base) apic_mock_set_bases(NULL, apic_ioapic_base);
    lapic_write(REG_TIMER_DIVIDE, divider);
    lapic_write(REG_LVT_TIMER, 32 | (1u << 17)); /* periodic at vector 32 */
    lapic_write(REG_TIMER_INITCNT, initial_count);
    klog_printf(KLOG_INFO, "lapic: timer div=%u init=%u", divider, initial_count);
    (void)lapic_read(REG_TIMER_CURRCNT);
}

void lapic_timer_calibrate(void) {
    if (!apic_lapic_base) apic_mock_set_bases(NULL, apic_ioapic_base);
    /* Divide by 128 for calmer IRQ rate */
    lapic_write(REG_TIMER_DIVIDE, lapic_divider_code);
    lapic_write(REG_LVT_TIMER, (1u << 16)); /* masked */
    lapic_write(REG_TIMER_INITCNT, 0xFFFFFFFF);

    /* Wait ~50ms using PIT for better accuracy */
    pit_wait_ms(50);

    uint32_t elapsed = 0xFFFFFFFF - lapic_read(REG_TIMER_CURRCNT);
    lapic_ticks_per_ms = elapsed / 50;
    if (lapic_ticks_per_ms == 0) lapic_ticks_per_ms = 50000; /* conservative fallback */
    klog_printf(KLOG_INFO, "lapic: calib ticks_per_ms=%u (div=128)", lapic_ticks_per_ms);
}

void lapic_timer_start_1ms(void) {
    if (!lapic_ticks_per_ms) lapic_timer_calibrate();
    uint32_t ticks = lapic_ticks_per_ms ? lapic_ticks_per_ms : 100000;
    lapic_write(REG_TIMER_DIVIDE, lapic_divider_code); /* divide by 128 */
    lapic_write(REG_LVT_TIMER, 32 | (1u << 17)); /* periodic vector 32 */
    lapic_write(REG_TIMER_INITCNT, ticks);
    klog_printf(KLOG_INFO, "lapic: timer 1ms div=128 init=%u", ticks);
    (void)lapic_read(REG_TIMER_CURRCNT);
}
