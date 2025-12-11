/**
 * Minimal MSI-X configuration helpers.
 */

#include "pci_msi.h"

void msix_init_entry(msix_entry_t *ent) {
    if (!ent) {
        return;
    }
    ent->vector = 32;
    ent->masked = false;
}

bool msix_enable(msix_entry_t *ent) {
    if (!ent) {
        return false;
    }
    ent->masked = false;
    if (ent->vector == 0) {
        ent->vector = 32;
    }
    return true;
}

bool msix_disable(msix_entry_t *ent) {
    if (!ent) {
        return false;
    }
    ent->masked = true;
    return true;
}
