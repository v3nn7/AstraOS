/**
 * PCI USB Port Routing Implementation
 * 
 * Forces USB ports to route to XHCI controller by writing to
 * USB3_PSSEN and XUSB2PR registers in PCI configuration space.
 * 
 * This is required on many chipsets (especially Intel) where USB ports
 * are not automatically routed to XHCI, causing Enable Slot TRB timeouts.
 */

#include "usb/pci_usb_routing.h"
#include "klog.h"
#include "interrupts.h"

/* PCI Configuration Space Access */
#define PCI_CONFIG_ADDRESS 0xCF8
#define PCI_CONFIG_DATA    0xCFC

/* PCI register offsets for USB port routing */
#define PCI_USB3_PSSEN     0xD0  /* USB3 Port SuperSpeed Enable */
#define PCI_XUSB2PR        0xD8  /* USB2 Port Routing */

/* PCI Programming Interface codes */
#define PCI_PROGIF_XHCI    0x30

/**
 * Read 32-bit value from PCI configuration space
 */
static uint32_t pci_read32(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset) {
    uint32_t addr = (1U << 31) | ((uint32_t)bus << 16) | ((uint32_t)slot << 11) | ((uint32_t)func << 8) | (offset & 0xFC);
    outl(PCI_CONFIG_ADDRESS, addr);
    return inl(PCI_CONFIG_DATA);
}

/**
 * Write 32-bit value to PCI configuration space
 */
static void pci_write32(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset, uint32_t val) {
    uint32_t addr = (1U << 31) | ((uint32_t)bus << 16) | ((uint32_t)slot << 11) | ((uint32_t)func << 8) | (offset & 0xFC);
    outl(PCI_CONFIG_ADDRESS, addr);
    outl(PCI_CONFIG_DATA, val);
}

/**
 * Force USB port routing to XHCI controller
 * 
 * This function writes 0xFFFFFFFF to USB3_PSSEN and XUSB2PR registers
 * to route all USB 2.0 and USB 3.0 ports to the XHCI controller.
 * 
 * The function is safe to call even if the chipset doesn't support
 * these registers - it will simply exit without error.
 */
void xhci_force_port_routing(pci_device_t *dev) {
    if (!dev) {
        return;
    }
    
    /* Only process XHCI controllers */
    if (dev->prog_if != PCI_PROGIF_XHCI) {
        return;
    }
    
    klog_printf(KLOG_INFO, "xhci: forcing port routing for XHCI at %02x:%02x.%x",
                dev->bus, dev->slot, dev->func);
    
    /* Read current values to check if registers exist
     * If registers don't exist, reads will return 0xFFFFFFFF or 0x00000000
     * We'll write anyway - it's safe on chipsets that don't support these registers
     */
    uint32_t usb3_pssen_before = pci_read32(dev->bus, dev->slot, dev->func, PCI_USB3_PSSEN);
    uint32_t xusb2pr_before = pci_read32(dev->bus, dev->slot, dev->func, PCI_XUSB2PR);
    
    klog_printf(KLOG_DEBUG, "xhci: USB3_PSSEN before=0x%08x XUSB2PR before=0x%08x",
                usb3_pssen_before, xusb2pr_before);
    
    /* Write 0xFFFFFFFF to USB3_PSSEN (USB3 Port SuperSpeed Enable)
     * This enables all USB 3.0 ports and routes them to XHCI
     */
    pci_write32(dev->bus, dev->slot, dev->func, PCI_USB3_PSSEN, 0xFFFFFFFF);
    
    /* Write 0xFFFFFFFF to XUSB2PR (USB2 Port Routing)
     * This routes all USB 2.0 ports to XHCI (instead of EHCI/OHCI/UHCI)
     */
    pci_write32(dev->bus, dev->slot, dev->func, PCI_XUSB2PR, 0xFFFFFFFF);
    
    /* Verify writes */
    uint32_t usb3_pssen_after = pci_read32(dev->bus, dev->slot, dev->func, PCI_USB3_PSSEN);
    uint32_t xusb2pr_after = pci_read32(dev->bus, dev->slot, dev->func, PCI_XUSB2PR);
    
    klog_printf(KLOG_DEBUG, "xhci: USB3_PSSEN after=0x%08x XUSB2PR after=0x%08x",
                usb3_pssen_after, xusb2pr_after);
    
    /* Log success - even if registers don't exist, this is not an error
     * Some chipsets don't support these registers, and that's OK
     */
    if (usb3_pssen_after == 0xFFFFFFFF && xusb2pr_after == 0xFFFFFFFF) {
        klog_printf(KLOG_INFO, "xhci: port routing forced (USB2+USB3 -> XHCI)");
    } else {
        klog_printf(KLOG_DEBUG, "xhci: port routing registers may not be supported on this chipset");
    }
}

