#pragma once

#include <stdint.h>
#include "types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Initialize PCI config access. If ecam_base_phys is zero, try ACPI MCFG. */
void pci_config_init(uint64_t ecam_base_phys);

uint8_t  pci_cfg_read8(uint8_t bus, uint8_t slot, uint8_t func, uint16_t off);
uint16_t pci_cfg_read16(uint8_t bus, uint8_t slot, uint8_t func, uint16_t off);
uint32_t pci_cfg_read32(uint8_t bus, uint8_t slot, uint8_t func, uint16_t off);

void pci_cfg_write8(uint8_t bus, uint8_t slot, uint8_t func, uint16_t off, uint8_t val);
void pci_cfg_write16(uint8_t bus, uint8_t slot, uint8_t func, uint16_t off, uint16_t val);
void pci_cfg_write32(uint8_t bus, uint8_t slot, uint8_t func, uint16_t off, uint32_t val);

/* BAR helpers */
uint64_t pci_cfg_read_bar(uint8_t bus, uint8_t slot, uint8_t func, uint8_t bar_off, bool *is_64bit_out);
void pci_enable_busmaster(uint8_t bus, uint8_t slot, uint8_t func);

#ifdef __cplusplus
}
#endif
