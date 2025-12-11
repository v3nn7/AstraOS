#include "../include/xhci.h"

#include <stddef.h>
#include <stdint.h>

extern void* kmemalign(size_t alignment, size_t size);
extern void* memset(void* dest, int val, size_t n);
extern uintptr_t virt_to_phys(const void* p);

static uint32_t trb_control(uint32_t type, uint8_t cycle) {
    return (type << 10) | (cycle ? 1u : 0u);
}

bool xhci_ring_init(xhci_trb_ring_t* ring, uint32_t trb_count) {
    if (!ring || trb_count < 2) {
        return false;
    }
    ring->trbs = (xhci_trb_t*)kmemalign(64, sizeof(xhci_trb_t) * trb_count);
    if (!ring->trbs) {
        return false;
    }
    memset(ring->trbs, 0, sizeof(xhci_trb_t) * trb_count);
    ring->count = trb_count;
    ring->enqueue = 0;
    ring->dequeue = 0;
    ring->cycle = 1;

    /* Link TRB at end pointing back to start, toggle cycle on wrap. */
    uint64_t base_phys = virt_to_phys(ring->trbs);
    xhci_trb_t* link = &ring->trbs[trb_count - 1];
    link->parameter_low = (uint32_t)(base_phys & 0xFFFFFFFFu);
    link->parameter_high = (uint32_t)(base_phys >> 32);
    link->status = 0;
    link->control = trb_control(XHCI_TRB_LINK, ring->cycle) | (1u << 1); /* toggle cycle */
    return true;
}

xhci_trb_t* xhci_ring_enqueue(xhci_trb_ring_t* ring, uint32_t trb_type) {
    if (!ring || !ring->trbs) {
        return NULL;
    }
    uint32_t idx = ring->enqueue;
    xhci_trb_t* trb = &ring->trbs[idx];
    trb->control = trb_control(trb_type, ring->cycle);
    ring->enqueue = (ring->enqueue + 1) % ring->count;
    if (ring->enqueue == 0) {
        ring->cycle ^= 1u;
    }
    return trb;
}

bool xhci_ring_pop(xhci_trb_ring_t* ring, xhci_trb_t* out) {
    if (!ring || !ring->trbs || !out) {
        return false;
    }
    if (ring->dequeue == ring->enqueue) {
        return false;
    }
    xhci_trb_t* trb = &ring->trbs[ring->dequeue];
    *out = *trb;
    ring->dequeue = (ring->dequeue + 1) % ring->count;
    if (ring->dequeue == 0) {
        ring->cycle ^= 1u;
    }
    return true;
}
#include "../include/xhci.h"
#include "../include/xhci_trb.h"

/* TRB ring allocation and maintenance logic will be implemented here. */
