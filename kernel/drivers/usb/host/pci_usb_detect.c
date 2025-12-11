/**
 * PCI scanning for USB host controllers.
 * Uses pci_find_xhci (CF8/CFC) and calls xhci_pci_probe.
 */

#include <drivers/usb/xhci.h>
#include <drivers/PCI/pci_config.h>
#include "klog.h"
#include <stdint.h>

/* Simple bus scan for xHCI class code */
bool pci_find_xhci(uintptr_t* mmio_base_out, uint8_t* bus_out, uint8_t* slot_out, uint8_t* func_out) {
    for (uint8_t bus = 0; bus < 32; bus++) {
        for (uint8_t slot = 0; slot < 32; slot++) {
            for (uint8_t func = 0; func < 8; func++) {
                uint32_t id = pci_cfg_read32(bus, slot, func, 0x00);
                if (id == 0xFFFFFFFFu) continue;
                uint32_t class_code = pci_cfg_read32(bus, slot, func, 0x08);
                uint8_t base = (class_code >> 24) & 0xFF;
                uint8_t sub = (class_code >> 16) & 0xFF;
                uint8_t prog = (class_code >> 8) & 0xFF;
                if (base == 0x0C && sub == 0x03 && prog == 0x30) {
                    bool bar64 = false;
                    uint64_t bar0 = pci_cfg_read_bar(bus, slot, func, 0x10, &bar64);
                    if (mmio_base_out) *mmio_base_out = (uintptr_t)bar0;
                    if (bus_out) *bus_out = bus;
                    if (slot_out) *slot_out = slot;
                    if (func_out) *func_out = func;
                    return true;
                }
            }
        }
    }
    return false;
}

void pci_usb_detect_scan(void) {
    uintptr_t base = 0;
    uint8_t bus = 0, slot = 0, func = 0;
    if (!pci_find_xhci(&base, &bus, &slot, &func)) {
        klog_printf(KLOG_WARN, "pci: no xHCI found");
        return;
    }
    /* Let the xHCI driver perform full probe+registration. */
    if (xhci_pci_probe(bus, slot, func) != 0) {
        klog_printf(KLOG_ERROR, "pci: xHCI probe failed at %x:%x.%u", bus, slot, func);
        return;
    }
    klog_printf(KLOG_INFO, "pci: xHCI attached at %x:%x.%u base=%llx",
                (unsigned)bus, (unsigned)slot, (unsigned)func, (unsigned long long)base);
}
