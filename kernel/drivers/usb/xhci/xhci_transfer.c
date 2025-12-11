/**
 * xHCI transfer helpers (minimal stub).
 */

#include <drivers/usb/xhci.h>
#include "string.h"

bool xhci_build_control_transfer(xhci_trb_ring_t *ring, usb_transfer_t *transfer,
                                 const usb_setup_packet_t *setup, uint32_t flags) {
    (void)flags;
    if (!ring || !transfer || !setup) {
        return false;
    }
    memcpy(transfer->setup, setup, sizeof(usb_setup_packet_t));
    transfer->status = USB_TRANSFER_SUCCESS;
    transfer->actual_length = transfer->length;
    return true;
}
