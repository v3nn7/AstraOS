#pragma once

#include <stdint.h>

int pci_setup_interrupt(uint8_t bus, uint8_t slot, uint8_t func, uint8_t legacy_irq, uint8_t *vector);
int pci_disable_msi(uint8_t bus, uint8_t slot, uint8_t func);

