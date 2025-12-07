#pragma once

#include "types.h"

/**
 * PCI MSI/MSI-X Support
 * 
 * Provides functions to enable Message Signaled Interrupts for PCI devices.
 */

/* Enable MSI for PCI device */
int pci_enable_msi(uint8_t bus, uint8_t slot, uint8_t func, uint8_t vector, uint8_t *msi_vector);

/* Enable MSI-X for PCI device */
int pci_enable_msix(uint8_t bus, uint8_t slot, uint8_t func, uint8_t entry, uint8_t vector, uint8_t *msix_vector);

/* Disable MSI/MSI-X */
int pci_disable_msi(uint8_t bus, uint8_t slot, uint8_t func);

/* Setup interrupt (try MSI-X, then MSI, then legacy) */
int pci_setup_interrupt(uint8_t bus, uint8_t slot, uint8_t func, uint8_t legacy_irq, uint8_t *vector);






