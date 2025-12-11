/**
 * Minimal ACPI parsing helpers.
 */
#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Initialize ACPI parsing (idempotent). */
void acpi_init(void);

uint64_t acpi_get_lapic_address(void);
uint64_t acpi_get_hpet_address(void);
void acpi_get_ioapic(uint64_t *phys, uint32_t *gsi_base);
void acpi_get_pcie_ecam(uint64_t *phys_base, uint8_t *start_bus, uint8_t *end_bus);

#ifdef __cplusplus
}
#endif
