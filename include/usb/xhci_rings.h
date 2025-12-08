#pragma once

#include "types.h"
#include "usb/xhci_trb.h"
#include <stdbool.h>
#include <stdint.h>

/* Transfer Ring */
typedef struct xhci_transfer_ring {
    xhci_trb_t *trbs;
    uint32_t size; /* Number of TRBs */
    uint32_t dequeue;
    uint32_t enqueue;
    bool cycle_state;
    phys_addr_t phys_addr;
} xhci_transfer_ring_t;

/* Command Ring */
typedef struct {
    xhci_trb_t *trbs;
    uint32_t size;
    uint32_t dequeue;
    uint32_t enqueue;
    bool cycle_state;
    phys_addr_t phys_addr;
} xhci_command_ring_t;

