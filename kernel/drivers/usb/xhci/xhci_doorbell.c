/**
 * xHCI doorbell stubs.
 */

#include <drivers/usb/xhci.h>

void xhci_ring_cmd_doorbell(xhci_controller_t *xhci) {
    (void)xhci;
}

void xhci_ring_doorbell(xhci_controller_t *xhci, uint8_t slot, uint8_t endpoint, uint16_t stream_id) {
    (void)xhci;
    (void)slot;
    (void)endpoint;
    (void)stream_id;
}
