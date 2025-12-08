/**
 * XHCI Debug Utilities
 * 
 * TRB dumping and tracing functions for debugging XHCI operations
 */

#include "usb/xhci.h"
#include "usb/xhci_internal.h"
#include "klog.h"
#include "mmio.h"
#include "vmm.h"

/* Forward declarations */
void xhci_dump_trb_level(int log_level, const char *label, xhci_trb_t *trb);

/* Helper macros for MMIO access */
#define XHCI_READ32(regs, offset) mmio_read32((volatile uint32_t *)((uintptr_t)(regs) + (offset)))
#define XHCI_READ64(regs, offset) mmio_read64((volatile uint64_t *)((uintptr_t)(regs) + (offset)))

/**
 * Get human-readable string for TRB type
 */
const char *xhci_trb_type_str(uint8_t type) {
    switch (type) {
        case XHCI_TRB_TYPE_NORMAL: return "Normal";
        case XHCI_TRB_TYPE_SETUP_STAGE: return "Setup Stage";
        case XHCI_TRB_TYPE_DATA_STAGE: return "Data Stage";
        case XHCI_TRB_TYPE_STATUS_STAGE: return "Status Stage";
        case XHCI_TRB_TYPE_ISOCH: return "Isoch";
        case XHCI_TRB_TYPE_LINK: return "Link";
        case XHCI_TRB_TYPE_EVENT_DATA: return "Event Data";
        case XHCI_TRB_TYPE_NOOP: return "No-op";
        case XHCI_TRB_TYPE_ENABLE_SLOT: return "Enable Slot";
        case XHCI_TRB_TYPE_DISABLE_SLOT: return "Disable Slot";
        case XHCI_TRB_TYPE_ADDRESS_DEVICE: return "Address Device";
        case XHCI_TRB_TYPE_CONFIGURE_ENDPOINT: return "Configure Endpoint";
        case XHCI_TRB_TYPE_EVALUATE_CONTEXT: return "Evaluate Context";
        case XHCI_TRB_TYPE_RESET_ENDPOINT: return "Reset Endpoint";
        case XHCI_TRB_TYPE_STOP_ENDPOINT: return "Stop Endpoint";
        case XHCI_TRB_TYPE_SET_TR_DEQUEUE: return "Set TR Dequeue";
        case XHCI_TRB_TYPE_RESET_DEVICE: return "Reset Device";
        case XHCI_TRB_TYPE_FORCE_EVENT: return "Force Event";
        case XHCI_TRB_TYPE_NEGOTIATE_BANDWIDTH: return "Negotiate Bandwidth";
        case XHCI_TRB_TYPE_SET_LATENCY_TOLERANCE: return "Set Latency Tolerance";
        case XHCI_TRB_TYPE_GET_PORT_BANDWIDTH: return "Get Port Bandwidth";
        case XHCI_TRB_TYPE_FORCE_HEADER: return "Force Header";
        case XHCI_TRB_TYPE_NOOP_CMD: return "No-op Cmd";
        case 33: return "Command Completion Event";
        default: return "Unknown";
    }
}

/**
 * Dump TRB contents for debugging
 * log_level: KLOG_DEBUG for normal tracing, KLOG_ERROR for error cases
 */
void xhci_dump_trb(const char *label, xhci_trb_t *trb) {
    xhci_dump_trb_level(KLOG_DEBUG, label, trb);
}

/**
 * Dump TRB contents with specified log level
 */
void xhci_dump_trb_level(int log_level, const char *label, xhci_trb_t *trb) {
    if (!trb) {
        if (label && label[0]) {
            klog_printf(KLOG_ERROR, "TRB %s: NULL pointer", label);
        } else {
            klog_printf(KLOG_ERROR, "TRB: NULL pointer");
        }
        return;
    }
    
    uint8_t trb_type = (trb->control >> XHCI_TRB_TYPE_SHIFT) & XHCI_TRB_TYPE_MASK;
    uint8_t cycle = (trb->control & XHCI_TRB_CYCLE) ? 1 : 0;
    uint8_t ioc = (trb->control & XHCI_TRB_IOC) ? 1 : 0;
    uint8_t tc = (trb->control & XHCI_TRB_TC) ? 1 : 0; /* TC replaces non-existent CH bit */
    uint8_t isp = (trb->control & XHCI_TRB_ISP) ? 1 : 0;
    uint8_t idt = (trb->control & XHCI_TRB_IDT) ? 1 : 0;
    
    if (label && label[0]) {
        klog_printf(log_level, "TRB %s:", label);
    }
    klog_printf(log_level, "  parameter=0x%016llx", (unsigned long long)trb->parameter);
    klog_printf(log_level, "  status=0x%08x", trb->status);
    klog_printf(log_level, "  control=0x%08x", trb->control);
    klog_printf(log_level, "  type=%u (%s)", trb_type, xhci_trb_type_str(trb_type));
    klog_printf(log_level, "  cycle=%u IOC=%u TC=%u ISP=%u IDT=%u", cycle, ioc, tc, isp, idt);
}

/**
 * Dump Event TRB contents for debugging
 */
void xhci_dump_event_trb(const char *label, xhci_event_trb_t *event) {
    if (!event) {
        if (label && label[0]) {
            klog_printf(KLOG_ERROR, "Event TRB %s: NULL pointer", label);
        } else {
            klog_printf(KLOG_ERROR, "Event TRB: NULL pointer");
        }
        return;
    }
    
    uint8_t event_type = (event->control >> XHCI_EVENT_TRB_TYPE_SHIFT) & XHCI_EVENT_TRB_TYPE_MASK;
    uint8_t completion_code = event->status & XHCI_EVENT_TRB_COMPLETION_CODE_MASK;
    uint16_t transfer_length = (event->status >> XHCI_EVENT_TRB_TRANSFER_LENGTH_SHIFT) & XHCI_EVENT_TRB_TRANSFER_LENGTH_MASK;
    uint8_t slot_id = (event->control >> XHCI_EVENT_TRB_SLOT_ID_SHIFT) & XHCI_EVENT_TRB_SLOT_ID_MASK;
    uint8_t endpoint_id = (event->control >> XHCI_EVENT_TRB_ENDPOINT_ID_SHIFT) & XHCI_EVENT_TRB_ENDPOINT_ID_MASK;
    uint8_t cycle = (event->control & XHCI_EVENT_TRB_CYCLE_BIT) ? 1 : 0;
    
    if (label && label[0]) {
        klog_printf(KLOG_DEBUG, "Event TRB %s:", label);
    }
    klog_printf(KLOG_DEBUG, "  data=0x%016llx", (unsigned long long)event->data);
    klog_printf(KLOG_DEBUG, "  status=0x%08x", event->status);
    klog_printf(KLOG_DEBUG, "  control=0x%08x", event->control);
    klog_printf(KLOG_DEBUG, "  type=%u (%s)", event_type, xhci_trb_type_str(event_type));
    klog_printf(KLOG_DEBUG, "  completion_code=%u transfer_length=%u", completion_code, transfer_length);
    klog_printf(KLOG_DEBUG, "  slot_id=%u endpoint_id=%u cycle=%u", slot_id, endpoint_id, cycle);
}

/**
 * Trace entire command ring
 */
void xhci_trace_cmd_ring(xhci_controller_t *xhci) {
    if (!xhci || !xhci->cmd_ring.trbs) {
        klog_printf(KLOG_ERROR, "xhci-trace: invalid command ring");
        return;
    }
    
    klog_printf(KLOG_DEBUG, "=== XHCI Command Ring Trace ===");
    klog_printf(KLOG_DEBUG, "Ring size: %u TRBs", xhci->cmd_ring.size);
    klog_printf(KLOG_DEBUG, "Enqueue: %u, Dequeue: %u, Cycle: %u", 
                xhci->cmd_ring.enqueue, xhci->cmd_ring.dequeue, 
                xhci->cmd_ring.cycle_state ? 1 : 0);
    klog_printf(KLOG_DEBUG, "Physical addr: 0x%016llx", 
                (unsigned long long)xhci->cmd_ring.phys_addr);
    
    /* Dump CRCR register */
    uint64_t crcr = xhci->op_regs->CRCR;
    klog_printf(KLOG_DEBUG, "CRCR: 0x%016llx (RCS=%u CSS=%u CRR=%u CA=%u)",
                (unsigned long long)crcr,
                (crcr & XHCI_CRCR_RCS) ? 1 : 0,
                (crcr & XHCI_CRCR_CSS) ? 1 : 0,
                (crcr & XHCI_CRCR_CRR) ? 1 : 0,
                (crcr & XHCI_CRCR_CA) ? 1 : 0);
    
    /* Dump all TRBs in ring */
    uint32_t count = 0;
    for (uint32_t i = 0; i < xhci->cmd_ring.size && count < 32; i++) {
        xhci_trb_t *trb = &xhci->cmd_ring.trbs[i];
        
        /* Skip empty TRBs (all zeros) unless they're at enqueue/dequeue */
        if (trb->parameter == 0 && trb->status == 0 && trb->control == 0 && 
            i != xhci->cmd_ring.enqueue && i != xhci->cmd_ring.dequeue) {
            continue;
        }
        
        /* Build label */
        const char *enq_marker = (i == xhci->cmd_ring.enqueue) ? " [ENQ]" : "";
        const char *deq_marker = (i == xhci->cmd_ring.dequeue) ? " [DEQ]" : "";
        klog_printf(KLOG_DEBUG, "CMD[%u]%s%s:", i, enq_marker, deq_marker);
        xhci_dump_trb_level(KLOG_DEBUG, "", trb);
        count++;
    }
    klog_printf(KLOG_DEBUG, "=== End Command Ring Trace ===");
}

/**
 * Trace entire event ring
 */
void xhci_trace_event_ring(xhci_controller_t *xhci) {
    if (!xhci || !xhci->event_ring.trbs) {
        klog_printf(KLOG_ERROR, "xhci-trace: invalid event ring");
        return;
    }
    
    klog_printf(KLOG_DEBUG, "=== XHCI Event Ring Trace ===");
    klog_printf(KLOG_DEBUG, "Ring size: %u TRBs", xhci->event_ring.size);
    klog_printf(KLOG_DEBUG, "Enqueue: %u, Dequeue: %u, Cycle: %u",
                xhci->event_ring.enqueue, xhci->event_ring.dequeue,
                xhci->event_ring.cycle_state ? 1 : 0);
    klog_printf(KLOG_DEBUG, "Physical addr: 0x%016llx",
                (unsigned long long)xhci->event_ring.phys_addr);
    
    /* Dump ERDP register */
    uint64_t erdp = XHCI_READ64(xhci->rt_regs, XHCI_ERDP(0));
    klog_printf(KLOG_DEBUG, "ERDP: 0x%016llx (EHB=%u)",
                (unsigned long long)erdp,
                (erdp & XHCI_ERDP_EHB) ? 1 : 0);
    
    /* Dump ERSTBA and ERSTSZ */
    uint64_t erstba = XHCI_READ64(xhci->rt_regs, XHCI_ERSTBA(0));
    uint32_t erstsz = XHCI_READ32(xhci->rt_regs, XHCI_ERSTSZ(0)) & 0xFFFF;
    klog_printf(KLOG_DEBUG, "ERSTBA: 0x%016llx ERSTSZ: %u",
                (unsigned long long)erstba, erstsz);
    
    /* Dump recent events (last 16) */
    uint32_t start = (xhci->event_ring.dequeue > 16) ? 
                     (xhci->event_ring.dequeue - 16) : 0;
    uint32_t count = 0;
    for (uint32_t i = start; i < xhci->event_ring.size && count < 16; i++) {
        xhci_event_trb_t *event = &xhci->event_ring.trbs[i];
        
        /* Skip empty events */
        if (event->data == 0 && event->status == 0 && event->control == 0) {
            continue;
        }
        
        const char *deq_marker = (i == xhci->event_ring.dequeue) ? " [DEQ]" : "";
        klog_printf(KLOG_DEBUG, "EVT[%u]%s:", i, deq_marker);
        xhci_dump_event_trb("", event);
        count++;
    }
    klog_printf(KLOG_DEBUG, "=== End Event Ring Trace ===");
}

/**
 * Trace transfer ring for specific slot/endpoint
 */
void xhci_trace_transfer_ring(xhci_controller_t *xhci, uint32_t slot, uint32_t endpoint) {
    if (!xhci || slot == 0 || slot > 32 || endpoint > 31) {
        klog_printf(KLOG_ERROR, "xhci-trace: invalid slot=%u endpoint=%u", slot, endpoint);
        return;
    }
    
    xhci_transfer_ring_t *ring = xhci->transfer_rings[slot][endpoint];
    if (!ring || !ring->trbs) {
        klog_printf(KLOG_DEBUG, "xhci-trace: no transfer ring for slot=%u endpoint=%u", slot, endpoint);
        return;
    }
    
    klog_printf(KLOG_DEBUG, "=== XHCI Transfer Ring Trace (slot=%u ep=%u) ===", slot, endpoint);
    klog_printf(KLOG_DEBUG, "Ring size: %u TRBs", ring->size);
    klog_printf(KLOG_DEBUG, "Enqueue: %u, Dequeue: %u, Cycle: %u",
                ring->enqueue, ring->dequeue, ring->cycle_state ? 1 : 0);
    klog_printf(KLOG_DEBUG, "Physical addr: 0x%016llx",
                (unsigned long long)ring->phys_addr);
    
    /* Dump active TRBs */
    uint32_t count = 0;
    for (uint32_t i = 0; i < ring->size && count < 32; i++) {
        xhci_trb_t *trb = &ring->trbs[i];
        
        /* Skip empty TRBs */
        if (trb->parameter == 0 && trb->status == 0 && trb->control == 0 &&
            i != ring->enqueue && i != ring->dequeue) {
            continue;
        }
        
        const char *enq_marker = (i == ring->enqueue) ? " [ENQ]" : "";
        const char *deq_marker = (i == ring->dequeue) ? " [DEQ]" : "";
        klog_printf(KLOG_DEBUG, "TR[%u][%u][%u]%s%s:", slot, endpoint, i, enq_marker, deq_marker);
        xhci_dump_trb_level(KLOG_DEBUG, "", trb);
        count++;
    }
    klog_printf(KLOG_DEBUG, "=== End Transfer Ring Trace ===");
}

/**
 * Trace XHCI controller state (registers and rings)
 */
void xhci_trace_controller(xhci_controller_t *xhci) {
    if (!xhci) {
        klog_printf(KLOG_ERROR, "xhci-trace: invalid controller");
        return;
    }
    
    klog_printf(KLOG_DEBUG, "========== XHCI Controller Trace ==========");
    
    /* USBSTS and USBCMD */
    uint32_t usbsts = xhci->op_regs->USBSTS;
    uint32_t usbcmd = xhci->op_regs->USBCMD;
    klog_printf(KLOG_DEBUG, "USBSTS: 0x%08x (HCH=%u HSE=%u EINT=%u PCD=%u CNR=%u)",
                usbsts,
                (usbsts & XHCI_STS_HCH) ? 1 : 0,
                (usbsts & XHCI_STS_HSE) ? 1 : 0,
                (usbsts & XHCI_STS_EINT) ? 1 : 0,
                (usbsts & XHCI_STS_PCD) ? 1 : 0,
                (usbsts & XHCI_STS_CNR) ? 1 : 0);
    klog_printf(KLOG_DEBUG, "USBCMD: 0x%08x (RUN=%u HCRST=%u INTE=%u)",
                usbcmd,
                (usbcmd & XHCI_CMD_RUN) ? 1 : 0,
                (usbcmd & XHCI_CMD_HCRST) ? 1 : 0,
                (usbcmd & XHCI_CMD_INTE) ? 1 : 0);
    
    /* IMAN register */
    uint32_t iman = XHCI_READ32(xhci->rt_regs, XHCI_IMAN(0));
    klog_printf(KLOG_DEBUG, "IMAN[0]: 0x%08x (IE=%u IP=%u)",
                iman,
                (iman & (1 << 1)) ? 1 : 0,
                (iman & (1 << 0)) ? 1 : 0);
    
    /* DCBAAP */
    klog_printf(KLOG_DEBUG, "DCBAAP: 0x%016llx",
                (unsigned long long)xhci->op_regs->DCBAAP);
    
    /* Controller info */
    klog_printf(KLOG_DEBUG, "Slots: %u, Ports: %u, Interrupters: %u",
                xhci->num_slots, xhci->num_ports, xhci->max_interrupters);
    
    /* Trace rings */
    xhci_trace_cmd_ring(xhci);
    xhci_trace_event_ring(xhci);
    
    klog_printf(KLOG_DEBUG, "========== End Controller Trace ==========");
}

