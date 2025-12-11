/**
 * xHCI ring helpers (minimal stub).
 */

#include <drivers/usb/xhci.h>
#include "kmalloc.h"
#include "string.h"

bool xhci_ring_init(xhci_trb_ring_t *ring, uint32_t entries) {
    if (!ring || entries == 0) {
        return false;
    }
    memset(ring, 0, sizeof(*ring));
    ring->trbs = (xhci_trb_t *)kmalloc(sizeof(xhci_trb_t) * entries);
    if (!ring->trbs) {
        return false;
    }
    memset(ring->trbs, 0, sizeof(xhci_trb_t) * entries);
    ring->size = entries;
    ring->enqueue_idx = 0;
    ring->dequeue_idx = 0;
    ring->cycle_state = true;
    return true;
}
