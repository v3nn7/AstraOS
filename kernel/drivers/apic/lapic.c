#include "lapic.h"
#include "apic_timer.h"
#include "vmm.h"
#include "acpi.h"
#include "klog.h"

static volatile uint32_t* lapic_base = 0;

void lapic_write(uint32_t reg, uint32_t value) {
    if (!lapic_base) {
        return;
    }
    lapic_base[reg / 4] = value;
}

uint32_t lapic_read(uint32_t reg) {
    if (!lapic_base) {
        return 0;
    }
    return lapic_base[reg / 4];
}

void lapic_eoi() {
    lapic_write(LAPIC_EOI, 0);
}

void lapic_send_ipi(uint32_t apic_id, uint32_t flags) {
    if (!lapic_base) return;
    lapic_write(LAPIC_ICR_HIGH, apic_id << 24);
    lapic_write(LAPIC_ICR_LOW, flags);
}

uint32_t lapic_get_id(void) {
    if (!lapic_base) return 0;
    return lapic_read(LAPIC_ID) >> 24;
}

void lapic_mask_lint(void) {
    if (!lapic_base) return;
    lapic_write(LAPIC_LVT_LINT0, lapic_read(LAPIC_LVT_LINT0) | (1u << 16));
    lapic_write(LAPIC_LVT_LINT1, lapic_read(LAPIC_LVT_LINT1) | (1u << 16));
}

void lapic_timer_setup(uint8_t vector, uint32_t divide, uint32_t initial_count, bool periodic) {
    if (!lapic_base) return;
    uint32_t lvt = (vector & LAPIC_TIMER_VEC_MASK);
    if (periodic) lvt |= LAPIC_TIMER_PERIODIC;
    lapic_write(LAPIC_DIVIDE, divide);
    lapic_write(LAPIC_LVT_TIMER, lvt);
    lapic_write(LAPIC_INIT_COUNT, initial_count);
}

void lapic_init() {
    uint64_t phys = acpi_get_lapic_address();
    if (phys == 0) {
        phys = 0xFEE00000ULL; /* default xAPIC base */
    }
    lapic_base = (volatile uint32_t*)vmm_map_mmio(phys, 0x1000);
    if (!lapic_base) {
        klog_printf(KLOG_ERROR, "lapic: map failed");
        return;
    }

    lapic_write(LAPIC_SVR, 0x100 | 0xFF); // enable LAPIC + spurious vector 0xFF
    lapic_write(LAPIC_TPR, 0x00);         // allow all priorities
    lapic_mask_lint();                    // avoid legacy LINT until set
    lapic_write(LAPIC_LVT_TIMER, LAPIC_TIMER_MASK | 0xFE); // mask timer initially
    apic_timer_calibrate();
    klog_printf(KLOG_INFO, "lapic: mapped phys=0x%llx id=%u",
                (unsigned long long)phys, (unsigned)lapic_get_id());
}
