/**
 * XHCI Context Management
 * 
 * Input Context builder for Address Device command.
 * Sets up Slot Context and Endpoint 0 Context.
 */

#include "usb/xhci.h"
#include "usb/usb_core.h"
#include "kmalloc.h"
#include "kernel.h"
#include "klog.h"
#include "string.h"
#include "pmm.h"
#include "vmm.h"

/* Helper macros */
#define XHCI_READ32(regs, offset) mmio_read32((volatile uint32_t *)((uintptr_t)(regs) + (offset)))
#define XHCI_WRITE32(regs, offset, val) mmio_write32((volatile uint32_t *)((uintptr_t)(regs) + (offset)), val)
#define XHCI_READ64(regs, offset) mmio_read64((volatile uint64_t *)((uintptr_t)(regs) + (offset)))
#define XHCI_WRITE64(regs, offset, val) mmio_write64((volatile uint64_t *)((uintptr_t)(regs) + (offset)), val)

/* Context Entry Masks */
#define XHCI_CTX_ENTRY_SLOT     (1 << 0)
#define XHCI_CTX_ENTRY_EP(n)    (1 << (n + 1))

/* Slot Context Fields */
#define XHCI_SLOT_CTX_ROUTE_SHIFT    0
#define XHCI_SLOT_CTX_SPEED_SHIFT    20
#define XHCI_SLOT_CTX_MTT_SHIFT      25
#define XHCI_SLOT_CTX_HUB_SHIFT      26
#define XHCI_SLOT_CTX_ENTRIES_SHIFT  27
#define XHCI_SLOT_CTX_PORT_SHIFT     16
#define XHCI_SLOT_CTX_PARENT_SLOT_SHIFT 8
#define XHCI_SLOT_CTX_PARENT_PORT_SHIFT 0
#define XHCI_SLOT_CTX_TTT_SHIFT      10
#define XHCI_SLOT_CTX_INTERRUPTER_SHIFT 22
#define XHCI_SLOT_CTX_ADDRESS_SHIFT  0
#define XHCI_SLOT_CTX_STATE_SHIFT    27

/* Endpoint Context Fields */
#define XHCI_EP_CTX_STATE_SHIFT          0
#define XHCI_EP_CTX_MULT_SHIFT           8
#define XHCI_EP_CTX_MAX_PSTREAMS_SHIFT   10
#define XHCI_EP_CTX_LSA_SHIFT            15
#define XHCI_EP_CTX_INTERVAL_SHIFT       16
#define XHCI_EP_CTX_MAX_ESIT_HI_SHIFT    24
#define XHCI_EP_CTX_ERROR_COUNT_SHIFT    1
#define XHCI_EP_CTX_EP_TYPE_SHIFT        3
#define XHCI_EP_CTX_MAX_PACKET_SHIFT     16
#define XHCI_EP_CTX_MAX_BURST_SHIFT      8
#define XHCI_EP_CTX_MAX_ESIT_LO_SHIFT    0
#define XHCI_EP_CTX_AVG_TRB_LEN_SHIFT    16
#define XHCI_EP_CTX_DEQUEUE_SHIFT        4
#define XHCI_EP_CTX_INTERRUPTER_SHIFT    22

/* Endpoint States */
#define XHCI_EP_STATE_DISABLED   0
#define XHCI_EP_STATE_RUNNING    1
#define XHCI_EP_STATE_HALTED     2
#define XHCI_EP_STATE_STOPPED    3
#define XHCI_EP_STATE_ERROR      4

/**
 * Allocate and initialize Input Context
 */
xhci_input_context_t *xhci_input_context_alloc(void) {
    xhci_input_context_t *ctx = (xhci_input_context_t *)kmalloc(sizeof(xhci_input_context_t) + 64);
    if (!ctx) {
        klog_printf(KLOG_ERROR, "xhci_context: failed to allocate input context");
        return NULL;
    }
    
    /* Align to 64-byte boundary */
    uintptr_t addr = (uintptr_t)ctx;
    if (addr % 64 != 0) {
        addr = (addr + 63) & ~63;
        ctx = (xhci_input_context_t *)addr;
    }
    
    k_memset(ctx, 0, sizeof(xhci_input_context_t));
    return ctx;
}

/**
 * Free Input Context
 */
void xhci_input_context_free(xhci_input_context_t *ctx) {
    if (ctx) {
        kfree(ctx);
    }
}

/**
 * Set Slot Context in Input Context
 */
void xhci_input_context_set_slot(xhci_input_context_t *ctx, uint8_t slot_id, uint8_t root_port,
                                  uint8_t speed, uint8_t address, bool hub, uint8_t parent_slot,
                                  uint8_t parent_port) {
    if (!ctx) return;
    
    /* Clear slot context */
    k_memset(ctx->slot_context, 0, sizeof(ctx->slot_context));
    
    /* Set Add Context Flag for Slot */
    ctx->add_context_flags |= XHCI_CTX_ENTRY_SLOT;
    
    /* Slot Context Word 0: Route String, Speed, MTT, Hub, Context Entries */
    uint32_t word0 = 0;
    word0 |= (speed & 0xF) << XHCI_SLOT_CTX_SPEED_SHIFT;
    word0 |= (hub ? 1 : 0) << XHCI_SLOT_CTX_HUB_SHIFT;
    word0 |= (1 << XHCI_SLOT_CTX_ENTRIES_SHIFT); /* 1 endpoint (EP0) */
    ctx->slot_context[0] = word0;
    
    /* Slot Context Word 1: Max Exit Latency */
    ctx->slot_context[1] = 0; /* Not used for non-hub devices */
    
    /* Slot Context Word 2: Root Hub Port Number, Number of Ports */
    uint32_t word2 = 0;
    word2 |= (root_port & 0xFF) << XHCI_SLOT_CTX_PORT_SHIFT;
    ctx->slot_context[2] = word2;
    
    /* Slot Context Word 3: Parent Hub Slot ID, Parent Port Number */
    uint32_t word3 = 0;
    if (hub) {
        word3 |= (parent_slot & 0xFF) << XHCI_SLOT_CTX_PARENT_SLOT_SHIFT;
        word3 |= (parent_port & 0xFF) << XHCI_SLOT_CTX_PARENT_PORT_SHIFT;
    }
    ctx->slot_context[3] = word3;
    
    /* Slot Context Word 4: TTT, Reserved, Interrupter Target */
    uint32_t word4 = 0;
    word4 |= (0 & 0x3FF) << XHCI_SLOT_CTX_INTERRUPTER_SHIFT; /* Interrupter 0 */
    ctx->slot_context[4] = word4;
    
    /* Slot Context Word 5: USB Device Address, Reserved, Slot State */
    uint32_t word5 = 0;
    word5 |= (address & 0xFF) << XHCI_SLOT_CTX_ADDRESS_SHIFT;
    word5 |= (1 << XHCI_SLOT_CTX_STATE_SHIFT); /* Enabled state */
    ctx->slot_context[5] = word5;
    
    klog_printf(KLOG_DEBUG, "xhci_context: slot context set: slot=%u port=%u speed=%u addr=%u hub=%d",
                slot_id, root_port, speed, address, hub ? 1 : 0);
}

/**
 * Set Endpoint 0 Context in Input Context
 */
void xhci_input_context_set_ep0(xhci_input_context_t *ctx, xhci_transfer_ring_t *transfer_ring,
                                 uint16_t max_packet_size) {
    if (!ctx || !transfer_ring) return;
    
    /* Clear endpoint 0 context */
    k_memset(ctx->endpoint_context[0], 0, sizeof(ctx->endpoint_context[0]));
    
    /* Set Add Context Flag for Endpoint 0 */
    ctx->add_context_flags |= XHCI_CTX_ENTRY_EP(0);
    
    /* Get physical address of transfer ring */
    uint64_t tr_dequeue = transfer_ring->phys_addr;
    uint32_t cycle_state = transfer_ring->cycle_state ? 1 : 0;
    
    /* Endpoint Context Word 0: EP State, Reserved, Mult, Max P-Streams, LSA, Interval */
    uint32_t word0 = 0;
    word0 |= (XHCI_EP_STATE_RUNNING & 0x7) << XHCI_EP_CTX_STATE_SHIFT;
    word0 |= (0 & 0x3) << XHCI_EP_CTX_MULT_SHIFT; /* Mult = 0 */
    word0 |= (0 & 0x1F) << XHCI_EP_CTX_MAX_PSTREAMS_SHIFT; /* No streams */
    word0 |= (0 & 0x1) << XHCI_EP_CTX_LSA_SHIFT; /* Linear Stream Array = 0 */
    word0 |= (0 & 0xFF) << XHCI_EP_CTX_INTERVAL_SHIFT; /* Interval = 0 for control */
    ctx->endpoint_context[0][0] = word0;
    
    /* Endpoint Context Word 1: Max ESIT Payload Hi, Error Count, EP Type, Reserved, Max Packet Size */
    uint32_t word1 = 0;
    word1 |= (0 & 0x3) << XHCI_EP_CTX_ERROR_COUNT_SHIFT; /* Error count = 0 */
    word1 |= (USB_ENDPOINT_XFER_CONTROL & 0x7) << XHCI_EP_CTX_EP_TYPE_SHIFT; /* Control endpoint */
    word1 |= (max_packet_size & 0xFFFF) << XHCI_EP_CTX_MAX_PACKET_SHIFT;
    ctx->endpoint_context[0][1] = word1;
    
    /* Endpoint Context Word 2: Max Burst Size, Reserved, Max ESIT Payload Lo */
    uint32_t word2 = 0;
    word2 |= (0 & 0xFF) << XHCI_EP_CTX_MAX_BURST_SHIFT; /* Max burst = 0 */
    ctx->endpoint_context[0][2] = word2;
    
    /* Endpoint Context Word 3: Reserved, Average TRB Length */
    ctx->endpoint_context[0][3] = 0;
    
    /* Endpoint Context Word 4-5: TR Dequeue Pointer (64-bit) */
    ctx->endpoint_context[0][4] = (uint32_t)(tr_dequeue & 0xFFFFFFFF);
    ctx->endpoint_context[0][5] = (uint32_t)((tr_dequeue >> 32) & 0xFFFFFFFF);
    
    /* Endpoint Context Word 6: Reserved, Interrupter Target */
    uint32_t word6 = 0;
    word6 |= (0 & 0x3FF) << XHCI_EP_CTX_INTERRUPTER_SHIFT; /* Interrupter 0 */
    ctx->endpoint_context[0][6] = word6;
    
    /* Endpoint Context Word 7: Reserved */
    ctx->endpoint_context[0][7] = 0;
    
    /* Set DCS bit (Dequeue Cycle State) in TR Dequeue Pointer */
    ctx->endpoint_context[0][4] |= (cycle_state & 1) << 0;
    
    klog_printf(KLOG_DEBUG, "xhci_context: EP0 context set: max_packet=%u tr_dequeue=0x%016llx cycle=%u",
                max_packet_size, (unsigned long long)tr_dequeue, cycle_state);
}

/**
 * Get physical address of Input Context
 */
phys_addr_t xhci_input_context_get_phys(xhci_input_context_t *ctx) {
    if (!ctx) return 0;
    
    uint64_t virt_addr = (uint64_t)(uintptr_t)ctx;
    uint64_t phys_addr = vmm_virt_to_phys(virt_addr);
    if (phys_addr == 0) {
        /* Fallback: use HHDM offset if available */
        extern uint64_t pmm_hhdm_offset;
        if (pmm_hhdm_offset && virt_addr >= pmm_hhdm_offset) {
            phys_addr = virt_addr - pmm_hhdm_offset;
        }
    }
    
    return (phys_addr_t)phys_addr;
}

