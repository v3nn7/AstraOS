/**
 * PCI IRQ setup wrapper.
 */

#include "pci_irq.h"
#include "pci_msi.h"
#include "klog.h"

int pci_configure_irq(uint8_t bus, uint8_t slot, uint8_t func, uint8_t legacy_irq, uint8_t *vector_out) {
    klog_printf(KLOG_INFO, "pci: irq setup bus=%u slot=%u func=%u legacy_irq=%u",
                (unsigned)bus, (unsigned)slot, (unsigned)func, (unsigned)legacy_irq);
    return pci_setup_interrupt(bus, slot, func, legacy_irq, vector_out);
}
