/**
 * USB debug helpers (no-op stubs).
 */

#include <drivers/usb/util/usb_debug.h>
#include "klog.h"

void usb_dump_trb(const xhci_trb_t *trb) {
    (void)trb;
}

void usb_dump_event_trb(const xhci_event_trb_t *trb) {
    (void)trb;
}

void usb_print_device_tree(void) {}

void usb_print_xhci_state(xhci_controller_t *xhci) {
    (void)xhci;
}

void usb_print_transfer(const usb_transfer_t *transfer) {
    (void)transfer;
}
