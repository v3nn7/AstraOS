#pragma once

/**
 * PCI USB Port Routing
 * 
 * Forces USB ports to route to XHCI controller on chipsets that support
 * USB3_PSSEN and XUSB2PR registers.
 * 
 * This fixes the issue where Enable Slot TRB timeouts occur because
 * USB ports are not routed to the XHCI controller.
 */

#include "types.h"

/* PCI Device structure for routing functions */
typedef struct {
    uint8_t bus;
    uint8_t slot;
    uint8_t func;
    uint8_t prog_if;
} pci_device_t;

/**
 * Force USB port routing to XHCI controller
 * 
 * Writes 0xFFFFFFFF to USB3_PSSEN (0xD0) and XUSB2PR (0xD8) registers
 * to route all USB 2.0 and USB 3.0 ports to XHCI controller.
 * 
 * This function is safe to call even if the chipset doesn't support
 * these registers - it will simply exit without error.
 * 
 * @param dev PCI device structure (must have prog_if == 0x30 for XHCI)
 */
void xhci_force_port_routing(pci_device_t *dev);

