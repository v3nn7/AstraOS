/**
 * Minimal ACPI address providers for LAPIC/HPET.
 * Real firmware parsing not yet implemented.
 */

#include "acpi.h"

uint64_t acpi_get_lapic_address(void) {
    /* Default LAPIC base for xAPIC (QEMU/PC-compatible). */
    return 0xFEE00000ULL;
}

uint64_t acpi_get_hpet_address(void) {
    /* Common HPET base on PC/QEMU. */
    return 0xFED00000ULL;
}

void acpi_get_ioapic(uint64_t *phys, uint32_t *gsi_base) {
    /* Default IOAPIC base and GSI range start for PC/QEMU. */
    if (phys) *phys = 0xFEC00000ULL;
    if (gsi_base) *gsi_base = 0;
}
