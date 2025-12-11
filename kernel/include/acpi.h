/**
 * Minimal ACPI parsing helpers.
 */
#pragma once

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Initialize ACPI parsing (idempotent). */
void acpi_init(void);

uint64_t acpi_get_lapic_address(void);
uint64_t acpi_get_hpet_address(void);
void acpi_get_ioapic(uint64_t *phys, uint32_t *gsi_base);
void acpi_get_pcie_ecam(uint64_t *phys_base, uint8_t *start_bus, uint8_t *end_bus);
bool acpi_get_isa_irq_override(uint8_t src_irq, uint32_t *gsi_out, uint16_t *flags_out);

/* Get CPU cores from MADT */
uint8_t acpi_get_lapic_count(void);
bool acpi_get_lapic_entry(uint8_t index, uint8_t *acpi_cpu_id, uint8_t *apic_id, uint32_t *flags);

#ifdef __cplusplus
}
#endif
