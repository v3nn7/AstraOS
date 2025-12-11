/**
 * xHCI doorbell stubs.
 */

#include <drivers/usb/xhci.h>

/* Ring command doorbell (target 0) */
void xhci_ring_cmd_doorbell(xhci_controller_t *xhci) {
    if (!xhci || !xhci->doorbell_regs) return;
    volatile uint32_t *db = (volatile uint32_t *)xhci->doorbell_regs;
    db[0] = 0;
}

/* Ring specific slot/endpoint doorbell (endpoint is DCI) */
void xhci_ring_doorbell(xhci_controller_t *xhci, uint8_t slot, uint8_t endpoint, uint16_t stream_id) {
    if (!xhci || !xhci->doorbell_regs) return;
    volatile uint32_t *db = (volatile uint32_t *)xhci->doorbell_regs;
    db[slot] = ((uint32_t)stream_id << 16) | endpoint;
}
