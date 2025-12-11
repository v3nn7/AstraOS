#ifndef XHCI_CONTEXT_H
#define XHCI_CONTEXT_H

#include <stdint.h>

/* Lightweight xHCI context structures to be fleshed out later. */
typedef struct xhci_slot_context {
    uint32_t route_string;
    uint32_t speed;
    uint32_t reserved[3];
} xhci_slot_context_t;

typedef struct xhci_endpoint_context {
    uint32_t ep_state;
    uint32_t tr_dequeue_low;
    uint32_t tr_dequeue_high;
    uint32_t reserved;
} xhci_endpoint_context_t;

#endif /* XHCI_CONTEXT_H */
