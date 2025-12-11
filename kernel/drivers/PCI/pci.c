/**
 * Simple PCI scanner (CF8/CFC) for logging.
 */

#include "pci.h"
#include "pci_config.h"
#include "klog.h"

void pci_scan_log(void) {
    pci_config_init(0);
    klog_printf(KLOG_INFO, "pci: scanning (ECAM if available, else CF8/CFC)");
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
                klog_printf(KLOG_INFO, "pci: %x:%x.%u class=%x/%x/%x irq=%u bar0=%llx",
                            (unsigned)bus, (unsigned)dev, (unsigned)func,
                            base, sub, prog, irq, (unsigned long long)bar0);
                if (func == 0) {
                    uint32_t hdr = pci_cfg_read32(bus, dev, func, 0x0C);
                    if ((hdr & 0x00800000) == 0) break; // not multifunction
                }
            }
        }
    }
}
