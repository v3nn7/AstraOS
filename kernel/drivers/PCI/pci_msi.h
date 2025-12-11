/**
 * PCI MSI/MSI-X helper definitions.
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint32_t vector;
    bool masked;
} msi_config_t;

typedef struct {
    uint32_t vector;
    bool masked;
} msix_entry_t;

/* MSI helpers */
void msi_init_config(msi_config_t *cfg);
bool msi_enable(msi_config_t *cfg);
bool msi_disable(msi_config_t *cfg);

/* MSI-X helpers */
void msix_init_entry(msix_entry_t *ent);
bool msix_enable(msix_entry_t *ent);
bool msix_disable(msix_entry_t *ent);

/* Vector allocator */
void msi_allocator_reset(void);
int msi_allocator_next_vector(void);

/* PCI interrupt helpers (stubbed) */
int pci_enable_msi(uint8_t bus, uint8_t slot, uint8_t func, uint8_t vector, uint8_t *msi_vector);
int pci_enable_msix(uint8_t bus, uint8_t slot, uint8_t func, uint8_t entry, uint8_t vector, uint8_t *msix_vector);
int pci_disable_msi(uint8_t bus, uint8_t slot, uint8_t func);
int pci_setup_interrupt(uint8_t bus, uint8_t slot, uint8_t func, uint8_t legacy_irq, uint8_t *vector);

#ifdef __cplusplus
}
#endif
