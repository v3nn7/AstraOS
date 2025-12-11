/**
 * XHCI Transfer Ring Management
 * 
 * Implements TRB ring allocation, enqueueing, and dequeueing.
 * Handles command ring, event ring, and transfer rings.
 */

#include <drivers/usb/xhci.h>
#include <drivers/usb/usb_core.h>
#include "kmalloc.h"
#include "kernel.h"
#include "klog.h"
#include "string.h"
#include "pmm.h"
#include "mmio.h"

/* Helper macros for MMIO access */
#define XHCI_READ32(regs, offset) mmio_read32((volatile uint32_t *)((uintptr_t)(regs) + (offset)))
#define XHCI_WRITE32(regs, offset, val) mmio_write32((volatile uint32_t *)((uintptr_t)(regs) + (offset)), val)
#define XHCI_READ64(regs, offset) mmio_read64((volatile uint64_t *)((uintptr_t)(regs) + (offset)))
#define XHCI_WRITE64(regs, offset, val) mmio_write64((volatile uint64_t *)((uintptr_t)(regs) + (offset)), val)

/* Runtime Register Offsets */
#define XHCI_ERSTBA(n)          (0x20 + ((n) * 0x20))
#define XHCI_ERSTSZ(n)          (0x24 + ((n) * 0x20))
#define XHCI_ERDP(n)            (0x28 + ((n) * 0x20))
#define XHCI_IMAN(n)            (0x00 + ((n) * 0x20))
#define XHCI_IMOD(n)            (0x04 + ((n) * 0x20))
#define XHCI_ERSTSZ_ERST_SZ_MASK 0xFFFF
#define XHCI_ERDP_EHB           (1 << 3)
#define XHCI_CRCR_RCS           (1 << 0)
#define XHCI_CRCR_CS            (1 << 1)
#define XHCI_CRCR_CA            (1 << 2)

/* ERST Entry */
typedef struct PACKED {
    uint64_t ring_segment_base;
    uint32_t ring_segment_size;
    uint32_t reserved;
} xhci_erst_entry_t;

/* Helper macros */
#define XHCI_TRB_SIZE           16
#define XHCI_RING_SIZE          256  /* 256 TRBs per ring */
#define XHCI_RING_MASK          (XHCI_RING_SIZE - 1)

/**
 * Allocate and initialize a command ring
 */
static int xhci_command_ring_alloc(xhci_command_ring_t *ring, uint32_t size) {
    if (!ring || size == 0) return -1;
    
    /* Round up to power of 2 */
    uint32_t ring_size = 1;
    while (ring_size < size) {
        ring_size <<= 1;
    }
    
    /* Allocate TRBs (must be 16-byte aligned) */
    size_t trb_array_size = ring_size * XHCI_TRB_SIZE;
    ring->trbs = (xhci_trb_t *)kmalloc(trb_array_size + 64); /* Extra for alignment */
    if (!ring->trbs) {
        klog_printf(KLOG_ERROR, "xhci_ring: allocation failed");
        return -1;
    }
    
    /* Align to 64-byte boundary */
    uintptr_t addr = (uintptr_t)ring->trbs;
    if (addr % 64 != 0) {
        addr = (addr + 63) & ~63;
        ring->trbs = (xhci_trb_t *)addr;
    }
    
    k_memset(ring->trbs, 0, trb_array_size);
    
    ring->size = ring_size;
    ring->dequeue = 0;
    ring->enqueue = 0;
    ring->cycle_state = true; /* Start with cycle bit = 1 */
    ring->phys_addr = (phys_addr_t)(uintptr_t)ring->trbs; /* TODO: Get real physical address */
    
    klog_printf(KLOG_INFO, "xhci_ring: allocated command ring size=%u at %p", ring_size, ring->trbs);
    return 0;
}

/**
 * Allocate and initialize an event ring
 */
static int xhci_event_ring_alloc(xhci_event_ring_t *ring, uint32_t size) {
    if (!ring || size == 0) return -1;
    
    /* Round up to power of 2 */
    uint32_t ring_size = 1;
    while (ring_size < size) {
        ring_size <<= 1;
    }
    
    /* Allocate event TRBs */
    size_t trb_array_size = ring_size * sizeof(xhci_event_trb_t);
    ring->trbs = (xhci_event_trb_t *)kmalloc(trb_array_size + 64);
    if (!ring->trbs) {
        klog_printf(KLOG_ERROR, "xhci_ring: event ring allocation failed");
        return -1;
    }
    
    /* Align to 64-byte boundary */
    uintptr_t addr = (uintptr_t)ring->trbs;
    if (addr % 64 != 0) {
        addr = (addr + 63) & ~63;
        ring->trbs = (xhci_event_trb_t *)addr;
    }
    
    k_memset(ring->trbs, 0, trb_array_size);
    
    ring->size = ring_size;
    ring->dequeue = 0;
    ring->enqueue = 0;
    ring->cycle_state = true;
    ring->phys_addr = (phys_addr_t)(uintptr_t)ring->trbs;
    ring->segment_table = NULL;
    ring->segment_table_phys = 0;
    
    klog_printf(KLOG_INFO, "xhci_ring: allocated event ring size=%u at %p", ring_size, ring->trbs);
    return 0;
}

/**
 * Allocate and initialize a transfer ring
 */
static int xhci_transfer_ring_alloc(xhci_transfer_ring_t *ring, uint32_t size) {
    if (!ring || size == 0) return -1;
    
    /* Round up to power of 2 */
    uint32_t ring_size = 1;
    while (ring_size < size) {
        ring_size <<= 1;
    }
    
    /* Allocate TRBs */
    size_t trb_array_size = ring_size * XHCI_TRB_SIZE;
    ring->trbs = (xhci_trb_t *)kmalloc(trb_array_size + 64);
    if (!ring->trbs) {
        klog_printf(KLOG_ERROR, "xhci_ring: transfer ring allocation failed");
        return -1;
    }
    
    /* Align to 64-byte boundary */
    uintptr_t addr = (uintptr_t)ring->trbs;
    if (addr % 64 != 0) {
        addr = (addr + 63) & ~63;
        ring->trbs = (xhci_trb_t *)addr;
    }
    
    k_memset(ring->trbs, 0, trb_array_size);
    
    ring->size = ring_size;
    ring->dequeue = 0;
    ring->enqueue = 0;
    ring->cycle_state = true;
    ring->phys_addr = (phys_addr_t)(uintptr_t)ring->trbs;
    
    klog_printf(KLOG_INFO, "xhci_ring: allocated transfer ring size=%u at %p", ring_size, ring->trbs);
    return 0;
}

/**
 * Enqueue a TRB to command ring
 */
int xhci_cmd_ring_enqueue(xhci_command_ring_t *ring, xhci_trb_t *trb) {
    if (!ring || !trb || !ring->trbs) return -1;
    
    uint32_t index = ring->enqueue & XHCI_RING_MASK;
    
    /* Copy TRB */
    memcpy(&ring->trbs[index], trb, sizeof(xhci_trb_t));
    
    /* Set cycle bit */
    if (ring->cycle_state) {
        ring->trbs[index].control |= XHCI_TRB_CYCLE;
    } else {
        ring->trbs[index].control &= ~XHCI_TRB_CYCLE;
    }
    
    /* Update enqueue pointer */
    ring->enqueue++;
    
    /* Toggle cycle bit if we wrapped around */
    if ((ring->enqueue & XHCI_RING_MASK) == 0) {
        ring->cycle_state = !ring->cycle_state;
    }
    
    /* Memory barrier */
    __asm__ volatile("mfence" ::: "memory");
    
    return 0;
}

/**
 * Enqueue a TRB to transfer ring
 */
int xhci_transfer_ring_enqueue(xhci_transfer_ring_t *ring, xhci_trb_t *trb) {
    if (!ring || !trb || !ring->trbs) return -1;
    
    uint32_t index = ring->enqueue & XHCI_RING_MASK;
    
    /* Copy TRB */
    memcpy(&ring->trbs[index], trb, sizeof(xhci_trb_t));
    
    /* Set cycle bit */
    if (ring->cycle_state) {
        ring->trbs[index].control |= XHCI_TRB_CYCLE;
    } else {
        ring->trbs[index].control &= ~XHCI_TRB_CYCLE;
    }
    
    /* Update enqueue pointer */
    ring->enqueue++;
    
    /* Toggle cycle bit if we wrapped around */
    if ((ring->enqueue & XHCI_RING_MASK) == 0) {
        ring->cycle_state = !ring->cycle_state;
    }
    
    /* Memory barrier */
    __asm__ volatile("mfence" ::: "memory");
    
    return 0;
}

/**
 * Dequeue an event TRB from event ring
 */
int xhci_event_ring_dequeue(xhci_event_ring_t *ring, xhci_event_trb_t *trb) {
    if (!ring || !trb || !ring->trbs) return -1;
    
    uint32_t index = ring->dequeue & XHCI_RING_MASK;
    
    /* Check cycle bit */
    uint8_t cycle_bit = (ring->trbs[index].cycle) & 1;
    if (cycle_bit != ring->cycle_state) {
        return -1; /* Not ready yet */
    }
    
    /* Copy TRB */
    memcpy(trb, &ring->trbs[index], sizeof(xhci_event_trb_t));
    
    /* Update dequeue pointer */
    ring->dequeue++;
    
    /* Toggle cycle bit if we wrapped around */
    if ((ring->dequeue & XHCI_RING_MASK) == 0) {
        ring->cycle_state = !ring->cycle_state;
    }
    
    return 0;
}

/**
 * Initialize command ring
 */
int xhci_cmd_ring_init(xhci_controller_t *xhci) {
    if (!xhci) return -1;
    
    int ret = xhci_command_ring_alloc(&xhci->cmd_ring, XHCI_RING_SIZE);
    if (ret < 0) {
        return -1;
    }
    
    /* Set command ring pointer in operational registers */
    uint64_t cmd_ring_addr = xhci->cmd_ring.phys_addr;
    XHCI_WRITE64(xhci->op_regs, XHCI_CRCR, cmd_ring_addr | XHCI_CRCR_RCS);
    
    klog_printf(KLOG_INFO, "xhci: command ring initialized at %p", (void *)cmd_ring_addr);
    return 0;
}

/**
 * Initialize event ring
 */
int xhci_event_ring_init(xhci_controller_t *xhci) {
    if (!xhci) return -1;
    
    int ret = xhci_event_ring_alloc(&xhci->event_ring, XHCI_RING_SIZE);
    if (ret < 0) {
        return -1;
    }
    
    /* Allocate event ring segment table */
    size_t erst_size = sizeof(xhci_erst_entry_t) * 1; /* Single segment for now */
    xhci_erst_entry_t *erst = (xhci_erst_entry_t *)kmalloc(erst_size);
    if (!erst) {
        kfree(xhci->event_ring.trbs);
        return -1;
    }
    
    k_memset(erst, 0, erst_size);
    
    /* Set up ERST entry */
    uint64_t event_ring_addr = xhci->event_ring.phys_addr;
    erst[0].ring_segment_base = event_ring_addr;
    erst[0].ring_segment_size = XHCI_RING_SIZE;
    
    xhci->event_ring.segment_table = erst;
    xhci->event_ring.segment_table_phys = (phys_addr_t)(uintptr_t)erst;
    
    /* Set ERST pointer in runtime registers */
    uint64_t erst_addr = xhci->event_ring.segment_table_phys;
    XHCI_WRITE64(xhci->rt_regs, XHCI_ERSTBA(0), erst_addr);
    XHCI_WRITE32(xhci->rt_regs, XHCI_ERSTSZ(0), 1); /* 1 segment */
    
    /* Set event ring dequeue pointer */
    XHCI_WRITE64(xhci->rt_regs, XHCI_ERDP(0), event_ring_addr | XHCI_ERDP_EHB);
    
    klog_printf(KLOG_INFO, "xhci: event ring initialized at %p", (void *)event_ring_addr);
    return 0;
}

/**
 * Initialize transfer ring for an endpoint
 */
int xhci_transfer_ring_init(xhci_controller_t *xhci, uint32_t slot, uint32_t endpoint) {
    if (!xhci || slot >= 32 || endpoint >= 32) return -1;
    
    if (xhci->transfer_rings[slot][endpoint]) {
        /* Already initialized */
        return 0;
    }
    
    xhci_transfer_ring_t *ring = (xhci_transfer_ring_t *)kmalloc(sizeof(xhci_transfer_ring_t));
    if (!ring) {
        return -1;
    }
    
    k_memset(ring, 0, sizeof(xhci_transfer_ring_t));
    
    int ret = xhci_transfer_ring_alloc(ring, XHCI_RING_SIZE);
    if (ret < 0) {
        kfree(ring);
        return -1;
    }
    
    xhci->transfer_rings[slot][endpoint] = ring;
    
    klog_printf(KLOG_INFO, "xhci: transfer ring slot=%u ep=%u initialized", slot, endpoint);
    return 0;
}

/**
 * Free transfer ring for an endpoint
 */
void xhci_transfer_ring_free(xhci_controller_t *xhci, uint32_t slot, uint32_t endpoint) {
    if (!xhci || slot >= 32 || endpoint >= 32) return;
    
    if (xhci->transfer_rings[slot][endpoint]) {
        xhci_transfer_ring_t *ring = xhci->transfer_rings[slot][endpoint];
        if (ring->trbs) {
            kfree(ring->trbs);
        }
        kfree(ring);
        xhci->transfer_rings[slot][endpoint] = NULL;
    }
}

/**
 * Build a TRB from parameters
 */
void xhci_build_trb(xhci_trb_t *trb, uint64_t data_ptr, uint32_t length, uint32_t type, uint32_t flags) {
    if (!trb) return;
    
    k_memset(trb, 0, sizeof(xhci_trb_t));
    
    trb->parameter = data_ptr;
    trb->status = length | (flags << 16);
    trb->control = type | (1 << 10); /* IOC bit */
}

/**
 * Build a Link TRB
 */
void xhci_build_link_trb(xhci_trb_t *trb, uint64_t next_ring_addr, uint8_t toggle_cycle) {
    if (!trb) return;
    
    k_memset(trb, 0, sizeof(xhci_trb_t));
    
    trb->parameter = next_ring_addr;
    trb->control = (XHCI_TRB_TYPE_LINK << XHCI_TRB_TYPE_SHIFT) | 
                   (toggle_cycle ? XHCI_TRB_TC : 0);
}

