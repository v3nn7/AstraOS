/**
 * Simple PCI scanner (CF8/CFC) for logging.
 */

#include "pci.h"
#include "klog.h"
#include "../../util/io.hpp"

static inline uint32_t pci_cfg_read32(uint8_t bus, uint8_t dev, uint8_t func, uint8_t off) {
    uint32_t addr = (1u << 31) |
                    ((uint32_t)bus << 16) |
                    ((uint32_t)dev << 11) |
                    ((uint32_t)func << 8) |
                    (off & 0xFC);
    outl(0xCF8, addr);
    return inl(0xCFC);
}

void pci_scan_log(void) {
    klog_printf(KLOG_INFO, "pci: scanning (CF8/CFC)");
    for (uint16_t bus = 0; bus < 256; ++bus) {
        for (uint8_t dev = 0; dev < 32; ++dev) {
            for (uint8_t func = 0; func < 8; ++func) {
                uint32_t id = pci_cfg_read32(bus, dev, func, 0x00);
                if (id == 0xFFFFFFFFu) {
                    if (func == 0) break;
                    continue;
                }
                uint32_t class_code = pci_cfg_read32(bus, dev, func, 0x08);
                uint8_t base = (class_code >> 24) & 0xFF;
                uint8_t sub = (class_code >> 16) & 0xFF;
                uint8_t prog = (class_code >> 8) & 0xFF;
                uint8_t irq = pci_cfg_read32(bus, dev, func, 0x3C) & 0xFF;
                uint32_t bar0 = pci_cfg_read32(bus, dev, func, 0x10);
                klog_printf(KLOG_INFO, "pci: %02x:%02x.%u class=%02x/%02x/%02x irq=%u bar0=0x%08x",
                            (unsigned)bus, (unsigned)dev, (unsigned)func,
                            base, sub, prog, irq, bar0);
                if (func == 0) {
                    uint32_t hdr = pci_cfg_read32(bus, dev, func, 0x0C);
                    if ((hdr & 0x00800000) == 0) break; // not multifunction
                }
            }
        }
    }
}
