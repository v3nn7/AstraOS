#include "ioapic.h"
#include "vmm.h"
#include "acpi.h"

#define IOAPIC_REGSEL  0x00
#define IOAPIC_WIN     0x10

static volatile uint32_t* ioapic = 0;
static uint32_t gsi_base = 0;

void ioapic_write(uint8_t reg, uint32_t val) {
    ioapic[IOAPIC_REGSEL / 4] = reg;
    ioapic[IOAPIC_WIN / 4] = val;
}

uint32_t ioapic_read(uint8_t reg) {
    ioapic[IOAPIC_REGSEL / 4] = reg;
    return ioapic[IOAPIC_WIN / 4];
}

uint32_t ioapic_get_gsi_base() {
    return gsi_base;
}

void ioapic_redirect_irq(uint8_t irq, uint32_t apic_id, uint8_t vector) {
    uint32_t idx = 0x10 + irq * 2;

    ioapic_write(idx, vector);              // low dword
    ioapic_write(idx + 1, apic_id << 24);   // high dword
}

void ioapic_init() {
    uint64_t phys;
    acpi_get_ioapic(&phys, &gsi_base);

    if (phys == 0) {
        /* No IOAPIC reported; skip init. */
        return;
    }

    ioapic = (volatile uint32_t*)vmm_map_mmio(phys, 0x20);
}
