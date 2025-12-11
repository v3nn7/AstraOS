#include "ioapic.h"
#include "vmm.h"
#include "acpi.h"
#include "klog.h"

#define IOAPIC_REGSEL  0x00
#define IOAPIC_WIN     0x10

static volatile uint32_t* ioapic = 0;
static uint32_t gsi_base = 0;
static uint32_t redir_count = 0;

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

static uint32_t translate_irq_to_gsi(uint8_t irq, uint16_t *flags_out) {
    uint32_t gsi = irq;
    uint16_t flags = 0;
    if (acpi_get_isa_irq_override(irq, &gsi, &flags)) {
        if (flags_out) *flags_out = flags;
    } else if (flags_out) {
        *flags_out = 0;
    }
    return gsi;
}

static uint32_t ioapic_redir_index(uint32_t gsi) {
    if (gsi < gsi_base) return (uint32_t)-1;
    uint32_t idx = gsi - gsi_base;
    if (redir_count && idx >= redir_count) return (uint32_t)-1;
    return 0x10 + idx * 2;
}

void ioapic_redirect_irq(uint8_t irq, uint32_t apic_id, uint8_t vector) {
    if (!ioapic) return;
    uint16_t flags = 0;
    uint32_t gsi = translate_irq_to_gsi(irq, &flags);
    uint32_t idx = ioapic_redir_index(gsi);
    if (idx == (uint32_t)-1) return;

    uint32_t low = vector;
    /* polarity: flags bit1 set -> active low */
    if (flags & 0x2) low |= (1u << 13);
    /* trigger: flags bit3 set -> level */
    if (flags & 0x8) low |= (1u << 15);

    ioapic_write(idx, low);
    ioapic_write(idx + 1, apic_id << 24);
    klog_printf(KLOG_INFO, "ioapic: redirect irq=%u(gsi=%u) vec=0x%02x apic_id=%u flags=0x%04x",
                irq, gsi, vector, apic_id, flags);
}

void ioapic_mask_irq(uint8_t irq) {
    if (!ioapic) return;
    uint32_t idx = ioapic_redir_index(translate_irq_to_gsi(irq, NULL));
    if (idx == (uint32_t)-1) return;
    uint32_t low = ioapic_read(idx);
    low |= (1u << 16); // mask
    ioapic_write(idx, low);
}

void ioapic_unmask_irq(uint8_t irq) {
    if (!ioapic) return;
    uint32_t idx = ioapic_redir_index(translate_irq_to_gsi(irq, NULL));
    if (idx == (uint32_t)-1) return;
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
    /* discover redirection entry count */
    uint32_t ver = ioapic_read(0x01);
    redir_count = ((ver >> 16) & 0xFF) + 1; /* n entries */
    klog_printf(KLOG_INFO, "ioapic: mapped phys=0x%llx gsi_base=%u", (unsigned long long)phys, gsi_base);
}
