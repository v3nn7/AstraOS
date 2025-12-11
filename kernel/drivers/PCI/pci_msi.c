/**
 * Minimal MSI configuration helpers.
 */

#include "pci_msi.h"
#include "klog.h"
#include <drivers/apic/lapic.h>

static uint32_t msi_message_address(void) {
    uint32_t apic = lapic_get_id();
    return 0xFEE00000u | (apic << 12);
}

void msi_init_config(msi_config_t *cfg) {
    if (!cfg) {
        return;
    }
    cfg->vector = 32;
    cfg->masked = false;
}

bool msi_enable(msi_config_t *cfg) {
    if (!cfg) {
        return false;
    }
    cfg->masked = false;
    if (cfg->vector == 0) {
        cfg->vector = 32;
    }
    klog_printf(KLOG_INFO, "msi: enable vec=%u addr=0x%08x data=0x%04x",
                cfg->vector, msi_message_address(), cfg->vector);
    return true;
}

bool msi_disable(msi_config_t *cfg) {
    if (!cfg) {
        return false;
    }
    cfg->masked = true;
    klog_printf(KLOG_INFO, "msi: disable");
    return true;
}

int pci_enable_msi(uint8_t bus, uint8_t slot, uint8_t func, uint8_t vector, uint8_t *msi_vector) {
    (void)bus; (void)slot; (void)func;
    uint8_t vec = vector ? vector : 32;
    if (msi_vector) {
        *msi_vector = vec;
    }
    klog_printf(KLOG_INFO, "msi: pci enable vec=%u addr=0x%08x", vec, msi_message_address());
    return 0; /* stub: not writing PCI capability */
}

int pci_enable_msix(uint8_t bus, uint8_t slot, uint8_t func, uint8_t entry, uint8_t vector, uint8_t *msix_vector) {
    (void)bus; (void)slot; (void)func; (void)entry;
    uint8_t vec = vector ? vector : 32;
    if (msix_vector) {
        *msix_vector = vec;
    }
    klog_printf(KLOG_INFO, "msix: pci enable vec=%u addr=0x%08x entry=%u", vec, msi_message_address(), entry);
    return 0; /* stub */
}

int pci_disable_msi(uint8_t bus, uint8_t slot, uint8_t func) {
    (void)bus;
    (void)slot;
    (void)func;
    klog_printf(KLOG_INFO, "msi: pci disable");
    return 0;
}

int pci_setup_interrupt(uint8_t bus, uint8_t slot, uint8_t func, uint8_t legacy_irq, uint8_t *vector) {
    (void)legacy_irq;
    uint8_t vec = vector ? *vector : 0;
    /* Prefer MSI (stub) */
    if (vec == 0) vec = 32;
    pci_enable_msi(bus, slot, func, vec, &vec);
    if (vector) *vector = vec;
    klog_printf(KLOG_INFO, "msi: setup bus=%u slot=%u func=%u vec=%u", bus, slot, func, vec);
    return 0; /* stub */
}
