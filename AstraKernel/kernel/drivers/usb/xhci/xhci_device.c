/**
 * XHCI Device Management
 * 
 * Enable Slot and Address Device commands.
 * These are required before any transfers can work.
 */

#include "usb/xhci.h"
#include "usb/usb_core.h"
#include "kmalloc.h"
#include "kernel.h"
#include "klog.h"
#include "string.h"
#include "mmio.h"
#include "vmm.h"

/* Forward declarations */
extern int xhci_cmd_ring_enqueue(xhci_command_ring_t *ring, xhci_trb_t *trb);
extern void xhci_ring_cmd_doorbell(xhci_controller_t *xhci);
extern int xhci_process_events(xhci_controller_t *xhci);
extern int xhci_transfer_ring_init(xhci_controller_t *xhci, uint32_t slot, uint32_t endpoint);
extern int xhci_event_ring_dequeue(xhci_event_ring_t *ring, xhci_event_trb_t *trb, void *rt_regs);

/* Helper macros */
#define XHCI_READ32(regs, offset) mmio_read32((volatile uint32_t *)((uintptr_t)(regs) + (offset)))
#define XHCI_WRITE32(regs, offset, val) mmio_write32((volatile uint32_t *)((uintptr_t)(regs) + (offset)), val)
#define XHCI_READ64(regs, offset) mmio_read64((volatile uint64_t *)((uintptr_t)(regs) + (offset)))
#define XHCI_WRITE64(regs, offset, val) mmio_write64((volatile uint64_t *)((uintptr_t)(regs) + (offset)), val)

/* Event TRB type for Command Completion (per XHCI spec) */
#ifndef XHCI_TRB_TYPE_COMMAND_COMPLETION
#define XHCI_TRB_TYPE_COMMAND_COMPLETION 33
#endif

/* Command Completion Event Tracking */
static uint32_t g_cmd_slot_id = 0;
static bool g_cmd_complete = false;
static uint8_t g_cmd_completion_code = 0;

/**
 * Process command completion events
 * Called from xhci_process_events() when command completion event is detected
 */
void xhci_handle_command_completion(xhci_controller_t *xhci, xhci_event_trb_t *event) {
    (void)xhci;  // Mark as unused for now
    uint8_t event_type = (event->control >> XHCI_EVENT_TRB_TYPE_SHIFT) & XHCI_EVENT_TRB_TYPE_MASK;
    
    klog_printf(KLOG_DEBUG, "xhci_device: handle_command_completion called: event_type=%u", event_type);
    
    if (event_type == 33) { /* Command Completion Event */
        uint8_t completion_code = event->status & XHCI_EVENT_TRB_COMPLETION_CODE_MASK;
        uint64_t cmd_trb_ptr = event->data;
        uint8_t slot_id = (event->control >> XHCI_EVENT_TRB_SLOT_ID_SHIFT) & XHCI_EVENT_TRB_SLOT_ID_MASK;
        
        klog_printf(KLOG_INFO, "xhci_device: Command Completion Event: code=%u slot_id=%u cmd_ptr=0x%016llx",
                    completion_code, slot_id, (unsigned long long)cmd_trb_ptr);
        
        g_cmd_complete = true;
        g_cmd_completion_code = completion_code;
        
        /* For Enable Slot command, slot ID is returned in completion event */
        if (completion_code == XHCI_TRB_CC_SUCCESS) {
            /* Slot ID is in the slot_id field of the event TRB for Enable Slot */
            if (slot_id > 0) {
                g_cmd_slot_id = slot_id;
            } else {
                /* Fallback: try to extract from command TRB pointer */
                g_cmd_slot_id = (uint32_t)(cmd_trb_ptr & 0xFF);
            }
            klog_printf(KLOG_INFO, "xhci_device: command SUCCESS: slot_id=%u", g_cmd_slot_id);
        } else {
            klog_printf(KLOG_ERROR, "xhci_device: command FAILED: code=%u", completion_code);
        }
    } else {
        klog_printf(KLOG_DEBUG, "xhci_device: non-command event type=%u", event_type);
    }
}

/**
 * Wait for command completion with timeout (microsecond-based loops)
 * Uses event ring dequeue with rt_regs to clear EHB properly.
 */
static int xhci_wait_for_command_completion(xhci_controller_t *xhci,
                                            xhci_event_trb_t *event_out,
                                            uint32_t timeout_us) {
    uint32_t loops = 0;
    uint32_t events_processed = 0;
    uint32_t max_loops = timeout_us; /* Approximate microsecond timing */
    
    /* FIRST: Check if interrupts are enabled */
    uint32_t iman = XHCI_READ32(xhci->rt_regs, XHCI_IMAN(0));
    klog_printf(KLOG_DEBUG, "xhci: waiting for command, IMAN=0x%08x (IE=%d, IP=%d)", 
                iman, (iman & (1 << 1)) ? 1 : 0, (iman & (1 << 0)) ? 1 : 0);
    
    if (!(iman & (1 << 1))) {
        klog_printf(KLOG_WARN, "xhci: interrupts are NOT enabled in IMAN!");
        /* Try to enable them */
        iman |= (1 << 1); /* IE bit */
        XHCI_WRITE32(xhci->rt_regs, XHCI_IMAN(0), iman);
        __asm__ volatile("mfence" ::: "memory");
    }
    
    /* SECOND: Clear any pending interrupts */
    XHCI_WRITE32(xhci->rt_regs, XHCI_IMAN(0), iman | (1 << 0)); /* Clear IP */
    
    while (loops++ < max_loops) {
        /* Check USBSTS for errors first */
        uint32_t usbsts = xhci->op_regs->USBSTS;
        if (usbsts & XHCI_STS_HSE) {
            klog_printf(KLOG_ERROR, "xhci: Host System Error detected! USBSTS=0x%08x", usbsts);
            return -1;
        }
        
        /* Poll for command completion event */
        int ret = xhci_process_events(xhci);
        if (ret > 0) {
            events_processed += ret;
            
            /* Check if our command completed via global flags */
            if (g_cmd_complete) {
                klog_printf(KLOG_DEBUG, "xhci: command completed after %u loops, %u events",
                            loops, events_processed);
                if (event_out && g_cmd_completion_code == XHCI_TRB_CC_SUCCESS) {
                    /* Build a minimal event TRB for caller */
                    k_memset(event_out, 0, sizeof(xhci_event_trb_t));
                    event_out->status = g_cmd_completion_code;
                    event_out->control = (g_cmd_slot_id & 0xFF) << XHCI_EVENT_TRB_SLOT_ID_SHIFT;
                    event_out->control |= (33 << XHCI_EVENT_TRB_TYPE_SHIFT); /* Command Completion */
                    event_out->control |= 1; /* Cycle bit */
                }
                return 0;
            }
        }
        
        /* Small delay to avoid tight spin */
        if (loops % 100 == 0) {
            /* Check USBSTS again */
            usbsts = xhci->op_regs->USBSTS;
            uint32_t new_iman = XHCI_READ32(xhci->rt_regs, XHCI_IMAN(0));
            
            if (usbsts & XHCI_STS_EINT) {
                klog_printf(KLOG_DEBUG, "xhci: USBSTS shows interrupt pending (EINT=1)");
            }
            if (new_iman & (1 << 0)) {
                klog_printf(KLOG_DEBUG, "xhci: IMAN shows interrupt pending (IP=1)");
            }
            
            for (volatile int i = 0; i < 100; i++);
        }
    }
    
    /* TIMEOUT - gather debug info */
    klog_printf(KLOG_ERROR, "xhci: command timeout after %u loops, %u events processed",
                loops, events_processed);
    
    /* Debug: CRCR and ERDP */
    uint64_t crcr = xhci->op_regs->CRCR;
    uint64_t erdp = XHCI_READ64(xhci->rt_regs, XHCI_ERDP(0));
    uint32_t usbsts = xhci->op_regs->USBSTS;
    uint32_t iman_after = XHCI_READ32(xhci->rt_regs, XHCI_IMAN(0));
    
    klog_printf(KLOG_ERROR, "xhci: CRCR after timeout: 0x%016llx RCS=%u CSS=%u CRR=%u",
                (unsigned long long)crcr,
                (crcr & XHCI_CRCR_RCS) ? 1 : 0,
                (crcr & XHCI_CRCR_CSS) ? 1 : 0,
                (crcr & XHCI_CRCR_CRR) ? 1 : 0);
    klog_printf(KLOG_ERROR, "xhci: ERDP=0x%016llx EHB=%u",
                (unsigned long long)erdp,
                (erdp & XHCI_ERDP_EHB) ? 1 : 0);
    klog_printf(KLOG_ERROR, "xhci: USBSTS=0x%08x (HCH=%d, CNR=%d, HSE=%d, EINT=%d)",
                usbsts,
                (usbsts & XHCI_STS_HCH) ? 1 : 0,
                (usbsts & XHCI_STS_CNR) ? 1 : 0,
                (usbsts & XHCI_STS_HSE) ? 1 : 0,
                (usbsts & XHCI_STS_EINT) ? 1 : 0);
    klog_printf(KLOG_ERROR, "xhci: IMAN after timeout: 0x%08x (IE=%d, IP=%d)",
                iman_after,
                (iman_after & (1 << 1)) ? 1 : 0,
                (iman_after & (1 << 0)) ? 1 : 0);
    
    if (xhci->cmd_ring.trbs) {
        xhci_trb_t *first_trb = &xhci->cmd_ring.trbs[0];
        xhci_dump_trb_level(KLOG_ERROR, "first TRB (timeout)", first_trb);
    }
    
    return -1;
}

/**
 * Enable Slot command
 * Returns slot ID on success, 0 on failure
 */
uint32_t xhci_enable_slot(xhci_controller_t *xhci) {
    if (!xhci) {
        klog_printf(KLOG_ERROR, "xhci_device: invalid controller");
        return 0;
    }
    
    klog_printf(KLOG_INFO, "xhci_device: enabling slot...");
    
    /* Build Enable Slot TRB */
    xhci_trb_t trb;
    k_memset(&trb, 0, sizeof(xhci_trb_t));
    trb.parameter = 0;
    trb.status = 0;
    trb.control = (XHCI_TRB_TYPE_ENABLE_SLOT << XHCI_TRB_TYPE_SHIFT);
    
    if (xhci_cmd_ring_enqueue(&xhci->cmd_ring, &trb) < 0) {
        klog_printf(KLOG_ERROR, "xhci_device: failed to enqueue Enable Slot TRB");
        return 0;
    }
    
    /* Ring command doorbell */
    xhci_ring_cmd_doorbell(xhci);
    __asm__ volatile("mfence" ::: "memory");
    
    /* Wait for completion event */
    xhci_event_trb_t event;
    if (xhci_wait_for_command_completion(xhci, &event, 1000000) < 0) {
        klog_printf(KLOG_ERROR, "xhci_device: Enable Slot command timed out");
        return 0;
    }
    
    uint8_t completion_code = event.status & XHCI_EVENT_TRB_COMPLETION_CODE_MASK;
    uint8_t slot_id = (event.control >> XHCI_EVENT_TRB_SLOT_ID_SHIFT) & XHCI_EVENT_TRB_SLOT_ID_MASK;
    
    if (completion_code != XHCI_TRB_CC_SUCCESS) {
        klog_printf(KLOG_ERROR, "xhci_device: Enable Slot failed code=%u", completion_code);
        return 0;
    }
    if (slot_id == 0 || slot_id > xhci->num_slots) {
        klog_printf(KLOG_ERROR, "xhci_device: invalid slot ID %u", slot_id);
        return 0;
    }
    
    klog_printf(KLOG_INFO, "xhci_device: slot %u enabled", slot_id);
    return slot_id;
}

/**
 * Address Device command
 * Sets up device context with Input Context
 */
int xhci_address_device(xhci_controller_t *xhci, uint32_t slot_id, xhci_input_context_t *input_ctx,
                        uint64_t input_ctx_phys) {
    if (!xhci || slot_id == 0 || slot_id > xhci->num_slots || !input_ctx) {
        klog_printf(KLOG_ERROR, "xhci_device: invalid parameters for Address Device");
        return -1;
    }
    
    klog_printf(KLOG_INFO, "xhci_device: addressing device slot=%u input_ctx_phys=0x%016llx",
                slot_id, (unsigned long long)input_ctx_phys);
    
    /* Initialize transfer ring for endpoint 0 if not already done */
    uint32_t ep_idx = 0;
    if (!xhci->transfer_rings[slot_id][ep_idx]) {
        if (xhci_transfer_ring_init(xhci, slot_id, ep_idx) < 0) {
            klog_printf(KLOG_ERROR, "xhci_device: failed to init transfer ring for EP0");
            return -1;
        }
    }
    
    /* Build Address Device TRB */
    xhci_trb_t trb;
    k_memset(&trb, 0, sizeof(xhci_trb_t));
    
    trb.parameter = input_ctx_phys; /* Physical address of Input Context */
    trb.status = (slot_id & 0xFF) | (XHCI_TRB_IOC << 16); /* Slot ID in lower 8 bits, IOC */
    trb.control = (XHCI_TRB_TYPE_ADDRESS_DEVICE << XHCI_TRB_TYPE_SHIFT) | XHCI_TRB_CYCLE;
    
    /* Enqueue to command ring */
    if (xhci_cmd_ring_enqueue(&xhci->cmd_ring, &trb) < 0) {
        klog_printf(KLOG_ERROR, "xhci_device: failed to enqueue Address Device TRB");
        return -1;
    }
    
    /* Ring command doorbell */
    xhci_ring_cmd_doorbell(xhci);
    
    /* Wait for completion */
    xhci_event_trb_t event;
    if (xhci_wait_for_command_completion(xhci, &event, 1000000) < 0) {
        klog_printf(KLOG_ERROR, "xhci_device: Address Device command failed");
        return -1;
    }
    
    /* Set DCBAA[slot_id] to Output Context (we'll allocate it later if needed) */
    /* For now, we just mark slot as allocated */
    xhci->slot_allocated[slot_id - 1] = 1;
    
    klog_printf(KLOG_INFO, "xhci_device: device addressed successfully (slot=%u)", slot_id);
    return 0;
}

