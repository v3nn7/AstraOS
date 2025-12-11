/**
 * Minimal ACPI stubs for LAPIC/HPET addresses.
 */
#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

uint64_t acpi_get_lapic_address(void);
uint64_t acpi_get_hpet_address(void);
void acpi_get_ioapic(uint64_t *phys, uint32_t *gsi_base);

#ifdef __cplusplus
}
#endif
