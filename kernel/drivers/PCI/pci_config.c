/**
 * PCI config space accessors with ECAM (if available) and CF8/CFC fallback.
 */

#include "pci_config.h"
#include "klog.h"
#include "acpi.h"
#include "mmio.h"
#include "../../util/io.hpp"

static uint64_t g_ecam_phys = 0;

static inline uintptr_t ecam_addr(uint8_t bus, uint8_t slot, uint8_t func, uint16_t off) {
    /* ECAM layout: bus<<20 | dev<<15 | func<<12 | offset */
    return (uintptr_t)(g_ecam_phys + ((uint64_t)bus << 20) + ((uint64_t)slot << 15) +
                       ((uint64_t)func << 12) + (off & 0xFFF));
}

static inline uint32_t cf8_addr(uint8_t bus, uint8_t slot, uint8_t func, uint16_t off) {
    return (1u << 31) |
           ((uint32_t)bus << 16) |
           ((uint32_t)slot << 11) |
           ((uint32_t)func << 8) |
           (off & 0xFC);
}

void pci_config_init(uint64_t ecam_base_phys) {
    if (ecam_base_phys == 0) {
        uint64_t base = 0;
        uint8_t start = 0, end = 0;
        acpi_get_pcie_ecam(&base, &start, &end);
        ecam_base_phys = base;
        (void)start;
        (void)end;
    }
    g_ecam_phys = ecam_base_phys;
    if (g_ecam_phys) {
        klog_printf(KLOG_INFO, "pci: ECAM base=0x%llx", (unsigned long long)g_ecam_phys);
    } else {
        klog_printf(KLOG_INFO, "pci: using CF8/CFC config access");
    }
}

uint32_t pci_cfg_read32(uint8_t bus, uint8_t slot, uint8_t func, uint16_t off) {
    if (g_ecam_phys) {
        volatile uint32_t *p = (volatile uint32_t *)ecam_addr(bus, slot, func, off);
        return mmio_read32(p);
    }
    uint32_t addr = cf8_addr(bus, slot, func, off);
    outl(0xCF8, addr);
    return inl(0xCFC);
}

uint16_t pci_cfg_read16(uint8_t bus, uint8_t slot, uint8_t func, uint16_t off) {
    uint32_t v = pci_cfg_read32(bus, slot, func, off & ~3u);
    uint8_t shift = (off & 2u) * 8u;
    return (uint16_t)((v >> shift) & 0xFFFFu);
}

uint8_t pci_cfg_read8(uint8_t bus, uint8_t slot, uint8_t func, uint16_t off) {
    uint32_t v = pci_cfg_read32(bus, slot, func, off & ~3u);
    uint8_t shift = (off & 3u) * 8u;
    return (uint8_t)((v >> shift) & 0xFFu);
}

void pci_cfg_write32(uint8_t bus, uint8_t slot, uint8_t func, uint16_t off, uint32_t val) {
    if (g_ecam_phys) {
        volatile uint32_t *p = (volatile uint32_t *)ecam_addr(bus, slot, func, off);
        mmio_write32(p, val);
        return;
    }
    uint32_t addr = cf8_addr(bus, slot, func, off);
    outl(0xCF8, addr);
    outl(0xCFC, val);
}

void pci_cfg_write16(uint8_t bus, uint8_t slot, uint8_t func, uint16_t off, uint16_t val) {
    uint32_t orig = pci_cfg_read32(bus, slot, func, off & ~3u);
    uint8_t shift = (off & 2u) * 8u;
    uint32_t mask = 0xFFFFu << shift;
    uint32_t combined = (orig & ~mask) | ((uint32_t)val << shift);
    pci_cfg_write32(bus, slot, func, off & ~3u, combined);
}

void pci_cfg_write8(uint8_t bus, uint8_t slot, uint8_t func, uint16_t off, uint8_t val) {
    uint32_t orig = pci_cfg_read32(bus, slot, func, off & ~3u);
    uint8_t shift = (off & 3u) * 8u;
    uint32_t mask = 0xFFu << shift;
    uint32_t combined = (orig & ~mask) | ((uint32_t)val << shift);
    pci_cfg_write32(bus, slot, func, off & ~3u, combined);
}

uint64_t pci_cfg_read_bar(uint8_t bus, uint8_t slot, uint8_t func, uint8_t bar_off, bool *is_64bit_out) {
    uint32_t lo = pci_cfg_read32(bus, slot, func, bar_off);
    if (is_64bit_out) *is_64bit_out = false;
    if (lo & 0x1) {
        return 0; /* I/O BAR not handled */
    }
    uint32_t type = (lo >> 1) & 0x3;
    uint64_t base = lo & 0xFFFFFFF0u;
    if (type == 0x2) {
        uint32_t hi = pci_cfg_read32(bus, slot, func, bar_off + 4);
        base |= ((uint64_t)hi) << 32;
        if (is_64bit_out) *is_64bit_out = true;
    }
    return base;
}

void pci_enable_busmaster(uint8_t bus, uint8_t slot, uint8_t func) {
    uint16_t cmd = pci_cfg_read16(bus, slot, func, 0x04);
    cmd |= 0x6; /* memory space + bus master */
    pci_cfg_write16(bus, slot, func, 0x04, cmd);
}
