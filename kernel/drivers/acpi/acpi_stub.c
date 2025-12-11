/**
 * Minimal ACPI address providers for LAPIC/HPET.
 * Real firmware parsing not yet implemented.
 */

#include "acpi.h"

uint64_t acpi_get_lapic_address(void) {
    /* Default LAPIC base for xAPIC. */
    return 0xFEE00000ULL;
}

uint64_t acpi_get_hpet_address(void) {
    /* Unknown HPET base; return 0 so callers can handle absence. */
    return 0;
}

void acpi_get_ioapic(uint64_t *phys, uint32_t *gsi_base) {
    if (phys) *phys = 0;
    if (gsi_base) *gsi_base = 0;
}
