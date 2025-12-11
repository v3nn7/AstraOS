#include "../include/xhci.h"
#include "../include/xhci_trb.h"
#include "../include/usb_transfer.h"

#include <stddef.h>
#include <stdint.h>

typedef struct xhci_transfer_ring {
    xhci_trb_ring_t ring;
} xhci_transfer_ring_t;

static void build_setup_trb(xhci_trb_t* trb, const usb_setup_packet_t* setup) {
    uint64_t data = ((uint64_t)setup->wValue << 48) |
                    ((uint64_t)setup->wIndex << 32) |
                    ((uint64_t)setup->wLength << 16) |
                    ((uint64_t)setup->bRequest << 8) |
                    ((uint64_t)setup->bmRequestType);
    trb->parameter_low = (uint32_t)(data & 0xFFFFFFFFu);
    trb->parameter_high = (uint32_t)(data >> 32);
    trb->status = 8;  // length of setup packet
    trb->control = (XHCI_TRB_SETUP_STAGE << 10) | 1u;
}

static void build_data_trb(xhci_trb_t* trb, const usb_transfer_t* xfer, uint64_t buffer_phys) {
    trb->parameter_low = (uint32_t)(buffer_phys & 0xFFFFFFFFu);
    trb->parameter_high = (uint32_t)(buffer_phys >> 32);
    trb->status = (uint32_t)xfer->length;
    trb->control = (XHCI_TRB_DATA_STAGE << 10) | 1u;
}

static void build_status_trb(xhci_trb_t* trb) {
    trb->parameter_low = 0;
    trb->parameter_high = 0;
    trb->status = 0;
    trb->control = (XHCI_TRB_STATUS_STAGE << 10) | 1u;
}

bool xhci_build_control_transfer(xhci_trb_ring_t* ring, const usb_transfer_t* xfer,
                                 const usb_setup_packet_t* setup, uint64_t buffer_phys) {
    if (!ring || !xfer || !setup) {
        return false;
    }
    xhci_trb_t* setup_trb = xhci_ring_enqueue(ring, XHCI_TRB_SETUP_STAGE);
    if (!setup_trb) return false;
    build_setup_trb(setup_trb, setup);

    if (xfer->length > 0) {
        xhci_trb_t* data_trb = xhci_ring_enqueue(ring, XHCI_TRB_DATA_STAGE);
        if (!data_trb) return false;
        build_data_trb(data_trb, xfer, buffer_phys);
    }

    xhci_trb_t* status_trb = xhci_ring_enqueue(ring, XHCI_TRB_STATUS_STAGE);
    if (!status_trb) return false;
    build_status_trb(status_trb);
    return true;
}
