#include "ioapic.h"
#include "vmm.h"
#include "acpi.h"
#include "klog.h"

#define IOAPIC_REGSEL  0x00
#define IOAPIC_WIN     0x10

static volatile uint32_t* ioapic = 0;
static uint32_t gsi_base = 0;

void ioapic_write(uint8_t reg, uint32_t val) {
    if (!ioapic) return;
    ioapic[IOAPIC_REGSEL / 4] = reg;
    ioapic[IOAPIC_WIN / 4] = val;
}

uint32_t ioapic_read(uint8_t reg) {
    if (!ioapic) return 0;
    ioapic[IOAPIC_REGSEL / 4] = reg;
    return ioapic[IOAPIC_WIN / 4];
}

uint32_t ioapic_get_gsi_base() {
    return gsi_base;
}

void ioapic_redirect_irq(uint8_t irq, uint32_t apic_id, uint8_t vector) {
    if (!ioapic) return;
    if (irq < gsi_base) return;
    uint32_t idx = 0x10 + (irq - gsi_base) * 2;

    ioapic_write(idx, vector);              // low dword
    ioapic_write(idx + 1, apic_id << 24);   // high dword
    klog_printf(KLOG_INFO, "ioapic: redirect irq=%u vec=0x%02x apic_id=%u", irq, vector, apic_id);
}

void ioapic_mask_irq(uint8_t irq) {
    if (!ioapic) return;
    if (irq < gsi_base) return;
    uint32_t idx = 0x10 + (irq - gsi_base) * 2;
    uint32_t low = ioapic_read(idx);
    low |= (1u << 16); // mask
    ioapic_write(idx, low);
}

void ioapic_unmask_irq(uint8_t irq) {
    if (!ioapic) return;
    if (irq < gsi_base) return;
    uint32_t idx = 0x10 + (irq - gsi_base) * 2;
    uint32_t low = ioapic_read(idx);
    low &= ~(1u << 16); // unmask
    ioapic_write(idx, low);
}

void ioapic_init() {
    uint64_t phys;
    acpi_get_ioapic(&phys, &gsi_base);

    if (phys == 0) {
        /* No IOAPIC reported; skip init. */
        klog_printf(KLOG_WARN, "ioapic: not found");
        return;
    }

    ioapic = (volatile uint32_t*)vmm_map_mmio(phys, 0x20);
    if (!ioapic) {
        klog_printf(KLOG_ERROR, "ioapic: map failed");
        return;
    }
    klog_printf(KLOG_INFO, "ioapic: mapped phys=0x%llx gsi_base=%u", (unsigned long long)phys, gsi_base);
}
