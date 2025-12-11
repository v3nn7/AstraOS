/**
 * Minimal MSI configuration helpers.
 */

#include "pci_msi.h"

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
    return true;
}

bool msi_disable(msi_config_t *cfg) {
    if (!cfg) {
        return false;
    }
    cfg->masked = true;
    return true;
}

int pci_enable_msi(uint8_t bus, uint8_t slot, uint8_t func, uint8_t vector, uint8_t *msi_vector) {
    (void)bus;
    (void)slot;
    (void)func;
    if (msi_vector) {
        *msi_vector = vector ? vector : 32;
    }
    return 0;
}

int pci_enable_msix(uint8_t bus, uint8_t slot, uint8_t func, uint8_t entry, uint8_t vector, uint8_t *msix_vector) {
    (void)bus;
    (void)slot;
    (void)func;
    (void)entry;
    if (msix_vector) {
        *msix_vector = vector ? vector : 32;
    }
    return 0;
}

int pci_disable_msi(uint8_t bus, uint8_t slot, uint8_t func) {
    (void)bus;
    (void)slot;
    (void)func;
    return 0;
}

int pci_setup_interrupt(uint8_t bus, uint8_t slot, uint8_t func, uint8_t legacy_irq, uint8_t *vector) {
    (void)bus;
    (void)slot;
    (void)func;
    /* Prefer MSI-X, then MSI, then fall back to legacy */
    if (vector) {
        *vector = legacy_irq ? legacy_irq : 32;
    }
    /* In host test stub we do not differentiate */
    return 0;
}
