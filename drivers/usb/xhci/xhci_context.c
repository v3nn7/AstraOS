/**
 * XHCI Context Management
 * 
 * Input Context builder for Address Device command.
 * Sets up Slot Context and Endpoint 0 Context.
 */

#include "usb/xhci.h"
#include "usb/xhci_internal.h"
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

/* Context Entry Masks are now in xhci_context.h */

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
    k_memset(&ctx->slot, 0, sizeof(xhci_slot_context_t));
    
    /* Set Add Context Flag for Slot */
    ctx->control.add_context_flags |= XHCI_CTX_ENTRY_SLOT;
    
    /* Fill Slot Context structure */
    ctx->slot.route_string = 0; /* Root hub port - no routing for SuperSpeed */
    ctx->slot.speed = speed & 0xF;
    ctx->slot.mtt = 0;
    ctx->slot.hub = hub ? 1 : 0;
    ctx->slot.context_entries = 1; /* 1 endpoint (EP0) */
    ctx->slot.max_exit_latency = 0;
    ctx->slot.root_hub_port_number = root_port;
    ctx->slot.num_ports = 0;
    ctx->slot.parent_hub_slot_id = hub ? parent_slot : 0;
    ctx->slot.parent_port_number = hub ? parent_port : 0;
    ctx->slot.ttt = 0;
    ctx->slot.interrupter_target = 0;
    ctx->slot.usb_device_address = address;
    ctx->slot.slot_state = XHCI_SLOT_STATE_ENABLED;
    
    klog_printf(KLOG_DEBUG, "xhci_context: slot context set: slot=%u port=%u speed=%u addr=%u hub=%d",
                slot_id, root_port, speed, address, hub ? 1 : 0);
}

/**
 * Set Endpoint 0 Context in Input Context
 */
void xhci_input_context_set_ep0(xhci_input_context_t *ctx, struct xhci_transfer_ring *transfer_ring,
                                 uint16_t max_packet_size) {
    /* Cast to xhci_transfer_ring_t for internal use */
    xhci_transfer_ring_t *ring = (xhci_transfer_ring_t *)transfer_ring;
    if (!ctx || !transfer_ring) return;
    
    /* Clear endpoint 0 context */
    k_memset(&ctx->endpoints[0], 0, sizeof(xhci_endpoint_context_t));
    
    /* Set Add Context Flag for Endpoint 0 */
    ctx->control.add_context_flags |= XHCI_CTX_ENTRY_EP(0);
    
    /* Get physical address of transfer ring */
    uint64_t tr_dequeue = ring->phys_addr;
    uint32_t cycle_state = ring->cycle_state ? 1 : 0;
    
    /* Fill Endpoint 0 Context structure */
    ctx->endpoints[0].ep_state = XHCI_EP_STATE_RUNNING;
    ctx->endpoints[0].mult = 0;
    ctx->endpoints[0].max_pstreams = 0;
    ctx->endpoints[0].lsa = 0;
    ctx->endpoints[0].interval = 0; /* Interval = 0 for control */
    ctx->endpoints[0].max_esit_payload_hi = 0;
    ctx->endpoints[0].error_count = 0;
    ctx->endpoints[0].ep_type = XHCI_EP_TYPE_CONTROL; /* Control Endpoint, bidirectional */
    ctx->endpoints[0].max_packet_size = max_packet_size;
    ctx->endpoints[0].max_burst_size = 0;
    ctx->endpoints[0].max_esit_payload_lo = 0;
    ctx->endpoints[0].average_trb_length = 0;
    /* Set dequeue pointer with DCS bit */
    ctx->endpoints[0].dcs = cycle_state & 1;
    ctx->endpoints[0].tr_dequeue_pointer_lo = (uint32_t)((tr_dequeue >> 4) & 0x0FFFFFFF);
    ctx->endpoints[0].tr_dequeue_pointer_hi = (uint32_t)(tr_dequeue >> 32);
    ctx->endpoints[0].interrupter_target = 0;
    
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

