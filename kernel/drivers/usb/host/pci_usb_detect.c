/**
 * PCI scanning for USB host controllers.
 * Uses pci_find_xhci (CF8/CFC) and calls xhci_pci_probe.
 */

#include <drivers/usb/xhci.h>
#include "klog.h"
#include <stdint.h>

/* Provided by kernel/util/memory.cpp */
extern bool pci_find_xhci(uintptr_t* mmio_base_out, uint8_t* bus_out, uint8_t* slot_out, uint8_t* func_out);

__attribute__((unused)) static void pci_usb_detect_scan(void) {
    uintptr_t base = 0;
    uint8_t bus = 0, slot = 0, func = 0;
    if (!pci_find_xhci(&base, &bus, &slot, &func)) {
        klog_printf(KLOG_WARN, "pci: no xHCI found");
        return;
    }
    /* Let the xHCI driver perform full probe+registration. */
    if (xhci_pci_probe(bus, slot, func) != 0) {
        klog_printf(KLOG_ERROR, "pci: xHCI probe failed at %02x:%02x.%u", bus, slot, func);
        return;
    }
    klog_printf(KLOG_INFO, "pci: xHCI attached at %02x:%02x.%u base=0x%llx",
                (unsigned)bus, (unsigned)slot, (unsigned)func, (unsigned long long)base);
}
