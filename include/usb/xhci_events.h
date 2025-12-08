#pragma once

#include "types.h"
#include "usb/xhci_trb.h"
#include <stdbool.h>
#include <stdint.h>

/* Event Ring Segment Table Entry */
typedef struct PACKED {
    uint64_t ring_segment_base;
    uint32_t ring_segment_size:16;
    uint32_t reserved:16;
    uint32_t reserved2;
} xhci_erst_entry_t;

/* Event Ring */
typedef struct {
    xhci_event_trb_t *trbs;
    uint32_t size;
    uint32_t dequeue;
    uint32_t enqueue;
    bool cycle_state;
    phys_addr_t phys_addr;
    phys_addr_t segment_table_phys;
    xhci_erst_entry_t *segment_table;
} xhci_event_ring_t;

