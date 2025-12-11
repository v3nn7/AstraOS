#include "../include/xhci.h"

#include <stddef.h>
#include <stdint.h>

extern void* kmemalign(size_t alignment, size_t size);
extern void* memset(void* dest, int val, size_t n);
extern uintptr_t virt_to_phys(const void* p);

bool xhci_event_ring_init(xhci_event_ring_t* evt, uint32_t trb_count) {
    if (!evt) {
        return false;
    }
    if (!xhci_ring_init(&evt->ring, trb_count)) {
        return false;
    }
    evt->erst_size = 1;
    evt->erst = (xhci_erst_entry_t*)kmemalign(64, sizeof(xhci_erst_entry_t) * evt->erst_size);
    if (!evt->erst) {
        return false;
    }
    memset(evt->erst, 0, sizeof(xhci_erst_entry_t) * evt->erst_size);
    uint64_t base = virt_to_phys(evt->ring.trbs);
    evt->erst[0].segment_base = base;
    evt->erst[0].segment_size = (uint16_t)trb_count;
    return true;
}

bool xhci_event_push(xhci_event_ring_t* evt, const xhci_trb_t* trb) {
    if (!evt || !trb) {
        return false;
    }
    uint8_t cycle = evt->ring.cycle;
    xhci_trb_t* slot = xhci_ring_enqueue(&evt->ring, (trb->control >> 10) & 0x3F);
    if (!slot) {
        return false;
    }
    *slot = *trb;
    slot->control = (trb->control & ~1u) | (cycle & 1u);
    return true;
}

bool xhci_event_pop(xhci_event_ring_t* evt, xhci_trb_t* out) {
    return xhci_ring_pop(&evt->ring, out);
}

uint32_t xhci_poll_events(xhci_controller_t* ctrl) {
    if (!ctrl) {
        return 0;
    }
    uint32_t processed = 0;
    xhci_trb_t evt;
    while (xhci_event_pop(&ctrl->evt_ring, &evt)) {
        ++processed;
    }
    return processed;
}

bool xhci_reap_and_arm(xhci_controller_t* ctrl) {
    if (!ctrl) {
        return false;
    }
    xhci_poll_events(ctrl);
    return true;
}
#include "../include/xhci.h"
#include "../include/xhci_trb.h"

/* Event ring consumption and dispatch will be implemented here. */
