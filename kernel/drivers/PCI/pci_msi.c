/**
 * Minimal MSI configuration helpers.
 */

#include "pci_msi.h"
#include "pci_config.h"
#include "klog.h"
#include <drivers/apic/lapic.h>

static uint32_t msi_message_address(void) {
    uint32_t apic = lapic_get_id();
    return 0xFEE00000u | (apic << 12);
}

static int find_capability(uint8_t bus, uint8_t slot, uint8_t func, uint8_t cap_id) {
    uint8_t status = pci_cfg_read16(bus, slot, func, 0x06) >> 8;
    if ((status & 0x10) == 0) return -1;
    uint8_t ptr = pci_cfg_read8(bus, slot, func, 0x34);
    int guard = 0;
    while (ptr && guard++ < 48) {
        uint8_t id = pci_cfg_read8(bus, slot, func, ptr);
        if (id == cap_id) return ptr;
        ptr = pci_cfg_read8(bus, slot, func, ptr + 1);
    }
    return -1;
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
    uint8_t vec = vector ? vector : 32;
    int cap = find_capability(bus, slot, func, 0x05);
    if (cap < 0) {
        klog_printf(KLOG_WARN, "msi: capability not found");
        return -1;
    }

    uint16_t ctrl = pci_cfg_read16(bus, slot, func, cap + 2);
    bool is64 = ctrl & (1u << 7);
    uint32_t msg_addr = msi_message_address();
    uint16_t msg_data = vec & 0xFF;

    pci_cfg_write32(bus, slot, func, cap + 4, msg_addr);
    if (is64) {
        pci_cfg_write32(bus, slot, func, cap + 8, 0);
        pci_cfg_write16(bus, slot, func, cap + 12, msg_data);
    } else {
        pci_cfg_write16(bus, slot, func, cap + 8, msg_data);
    }

    ctrl |= (1u << 0); // enable
    pci_cfg_write16(bus, slot, func, cap + 2, ctrl);

    if (msi_vector) *msi_vector = vec;
    klog_printf(KLOG_INFO, "msi: enabled bus=%u slot=%u func=%u vec=%u addr=0x%08x",
                bus, slot, func, vec, msg_addr);
    return 0;
}

int pci_enable_msix(uint8_t bus, uint8_t slot, uint8_t func, uint8_t entry, uint8_t vector, uint8_t *msix_vector) {
    uint8_t vec = vector ? vector : 32;
    int cap = find_capability(bus, slot, func, 0x11);
    if (cap < 0) {
        /* fallback to MSI */
        return pci_enable_msi(bus, slot, func, vector, msix_vector);
    }
    /* Minimal MSI-X: set function mask cleared and enable bit; table programming skipped */
    uint16_t ctrl = pci_cfg_read16(bus, slot, func, cap + 2);
    ctrl &= ~(1u << 14); /* clear function mask */
    ctrl |= (1u << 15);  /* enable MSI-X */
    pci_cfg_write16(bus, slot, func, cap + 2, ctrl);
    if (msix_vector) *msix_vector = vec;
    klog_printf(KLOG_INFO, "msix: enable (minimal) bus=%u slot=%u func=%u entry=%u vec=%u", bus, slot, func, entry, vec);
    return 0;
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
