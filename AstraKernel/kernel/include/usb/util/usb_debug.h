#pragma once

#include "usb/usb_core.h"
#include "usb/xhci.h"
#include "types.h"

/**
 * USB Debugging Tools
 * 
 * Provides functions for debugging USB stack.
 */

/* TRB dumping */
void usb_dump_trb(const xhci_trb_t *trb);
void usb_dump_event_trb(const xhci_event_trb_t *trb);

/* Device tree printing */
void usb_print_device_tree(void);

/* XHCI state printing */
void usb_print_xhci_state(xhci_controller_t *xhci);

/* Transfer printing */
void usb_print_transfer(const usb_transfer_t *transfer);


