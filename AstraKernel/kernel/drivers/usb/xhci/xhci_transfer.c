/**
 * XHCI Transfer Implementation
 * 
 * Full TRB-based control, interrupt, bulk, and isochronous transfers.
 */

#include "usb/xhci.h"
#include "usb/usb_core.h"
#include "usb/usb_transfer.h"
#include "kmalloc.h"
#include "kernel.h"
#include "klog.h"
#include "string.h"
#include "mmio.h"

/* Forward declaration */
struct usb_transfer;

/* Helper macros */
#define XHCI_READ32(regs, offset) mmio_read32((volatile uint32_t *)((uintptr_t)(regs) + (offset)))
#define XHCI_WRITE32(regs, offset, val) mmio_write32((volatile uint32_t *)((uintptr_t)(regs) + (offset)), val)
#define XHCI_READ64(regs, offset) mmio_read64((volatile uint64_t *)((uintptr_t)(regs) + (offset)))
#define XHCI_WRITE64(regs, offset, val) mmio_write64((volatile uint64_t *)((uintptr_t)(regs) + (offset)), val)

/* XHCI Transfer State - stored in transfer->controller_private */
typedef struct {
    bool setup_complete;
    bool data_complete;
    bool status_complete;
    uint32_t data_length; /* Actual data length from Data Stage */
    uint8_t completion_code; /* Last completion code */
    uint32_t slot;
    uint32_t endpoint;
} xhci_transfer_state_t;

/**
 * Wait for transfer completion with timeout
 */
static int xhci_wait_for_transfer_completion(xhci_controller_t *xhci, usb_transfer_t *transfer, 
                                             uint32_t timeout_ms, bool wait_setup, bool wait_data, bool wait_status) {
    if (!xhci || !transfer) return -1;
    
    xhci_transfer_state_t *state = (xhci_transfer_state_t *)transfer->controller_private;
    if (!state) return -1;
    
    uint32_t timeout_loops = timeout_ms * 1000; /* Convert ms to microsecond loops */
    uint32_t loops = 0;
    
    while (loops < timeout_loops) {
        /* Process events */
        extern int xhci_process_events(xhci_controller_t *xhci);
        xhci_process_events(xhci);
        
        /* Check if required stages are complete */
        bool setup_ok = !wait_setup || state->setup_complete;
        bool data_ok = !wait_data || state->data_complete;
        bool status_ok = !wait_status || state->status_complete;
        
        if (setup_ok && data_ok && status_ok) {
            /* All required stages completed */
            if (state->completion_code == XHCI_TRB_CC_SUCCESS) {
                return 0; /* Success */
            } else {
                transfer->status = USB_TRANSFER_ERROR;
                return -1; /* Error */
            }
        }
        
        /* Check for errors */
        if (state->completion_code != 0 && state->completion_code != XHCI_TRB_CC_SUCCESS) {
            transfer->status = USB_TRANSFER_ERROR;
            return -1;
        }
        
        /* Small delay */
        for (volatile int i = 0; i < 100; i++);
        loops += 100;
    }
    
    /* Timeout */
    transfer->status = USB_TRANSFER_TIMEOUT;
    klog_printf(KLOG_WARN, "xhci: transfer timeout (setup=%d data=%d status=%d)",
                state->setup_complete, state->data_complete, state->status_complete);
    return -1;
}

/**
 * Submit control transfer using TRBs
 * 
 * Control transfer consists of:
 * 1. Setup Stage TRB (in transfer ring for endpoint 0)
 * 2. Data Stage TRB (if wLength > 0, in transfer ring for endpoint 0)
 * 3. Status Stage TRB (in transfer ring for endpoint 0)
 * 
 * All stages go to the transfer ring for endpoint 0, not the command ring.
 */
int xhci_submit_control_transfer(xhci_controller_t *xhci, usb_transfer_t *transfer) {
    if (!xhci || !transfer || !transfer->device) return -1;

    usb_device_t *dev = transfer->device;
    uint32_t slot = dev->slot_id;
    
    if (slot == 0 || slot > xhci->num_slots) {
        klog_printf(KLOG_ERROR, "xhci: invalid slot %u for control transfer", slot);
        return -1;
    }
    
    /* Initialize transfer state */
    xhci_transfer_state_t *state = (xhci_transfer_state_t *)kmalloc(sizeof(xhci_transfer_state_t));
    if (!state) {
        klog_printf(KLOG_ERROR, "xhci: failed to allocate transfer state");
        return -1;
    }
    k_memset(state, 0, sizeof(xhci_transfer_state_t));
    state->slot = slot;
    state->endpoint = 0;
    transfer->controller_private = state;
    transfer->status = USB_TRANSFER_SUCCESS;
    transfer->actual_length = 0;
    
    /* Register as active transfer */
    xhci->active_control_transfers[slot - 1] = transfer;
    
    /* Get setup packet */
    uint8_t *setup = transfer->setup;
    uint16_t wLength = (setup[6] | (setup[7] << 8));
    uint8_t bmRequestType = setup[0];
    bool data_in = (bmRequestType & USB_ENDPOINT_DIR_IN) != 0;
    
    klog_printf(KLOG_DEBUG, "xhci: control transfer slot=%u wLength=%u data_in=%d",
                slot, wLength, data_in ? 1 : 0);
    
    /* Initialize transfer ring for endpoint 0 if needed */
    uint32_t ep_idx = 0; /* Control endpoint 0 */
    if (!xhci->transfer_rings[slot][ep_idx]) {
        extern int xhci_transfer_ring_init(xhci_controller_t *xhci, uint32_t slot, uint32_t endpoint);
        if (xhci_transfer_ring_init(xhci, slot, ep_idx) < 0) {
            klog_printf(KLOG_ERROR, "xhci: failed to init transfer ring for control endpoint");
            kfree(state);
            transfer->controller_private = NULL;
            xhci->active_control_transfers[slot - 1] = NULL;
            return -1;
        }
    }
    
    xhci_transfer_ring_t *transfer_ring = xhci->transfer_rings[slot][ep_idx];
    if (!transfer_ring) {
        klog_printf(KLOG_ERROR, "xhci: transfer ring not initialized");
        kfree(state);
        transfer->controller_private = NULL;
        xhci->active_control_transfers[slot - 1] = NULL;
        return -1;
    }
    
    /* Step 1: Setup Stage TRB (in transfer ring for endpoint 0) */
    xhci_trb_t setup_trb;
    k_memset(&setup_trb, 0, sizeof(xhci_trb_t));
    
    uint64_t setup_addr = (uint64_t)(uintptr_t)transfer->setup;
    setup_trb.parameter = setup_addr;
    setup_trb.status = 8 | (XHCI_TRB_IOC << 16); /* 8 bytes, interrupt on completion */
    setup_trb.control = (XHCI_TRB_TYPE_SETUP_STAGE << XHCI_TRB_TYPE_SHIFT) | XHCI_TRB_CYCLE | XHCI_TRB_IDT;
    
    /* Enqueue Setup Stage TRB to transfer ring (endpoint 0) */
    extern int xhci_transfer_ring_enqueue(xhci_transfer_ring_t *ring, xhci_trb_t *trb);
    if (xhci_transfer_ring_enqueue(transfer_ring, &setup_trb) < 0) {
        klog_printf(KLOG_ERROR, "xhci: failed to enqueue setup TRB");
        kfree(state);
        transfer->controller_private = NULL;
        xhci->active_control_transfers[slot - 1] = NULL;
        return -1;
    }
    
    /* Ring doorbell for endpoint 0 */
    extern void xhci_ring_doorbell(xhci_controller_t *xhci, uint8_t slot, uint8_t endpoint, uint16_t stream_id);
    xhci_ring_doorbell(xhci, slot, 0, 0);
    
    /* Wait for Setup Stage completion */
    if (xhci_wait_for_transfer_completion(xhci, transfer, 1000, true, false, false) < 0) {
        klog_printf(KLOG_ERROR, "xhci: setup stage failed");
        kfree(state);
        transfer->controller_private = NULL;
        xhci->active_control_transfers[slot - 1] = NULL;
        return -1;
    }
    
    /* Step 2: Data Stage TRB (if wLength > 0) */
    if (wLength > 0 && transfer->buffer) {
        xhci_trb_t data_trb;
        k_memset(&data_trb, 0, sizeof(xhci_trb_t));
        
        uint64_t data_addr = (uint64_t)(uintptr_t)transfer->buffer;
        data_trb.parameter = data_addr;
        data_trb.status = wLength | (XHCI_TRB_IOC << 16);
        
        /* Data Stage TRB type - use DATA_STAGE with direction in TRT field */
        uint32_t data_type = XHCI_TRB_TYPE_DATA_STAGE;
        uint32_t trt = data_in ? 2 : 1; /* TRT: 1=OUT, 2=IN */
        data_trb.control = (data_type << XHCI_TRB_TYPE_SHIFT) | (trt << XHCI_TRB_TRT_SHIFT) | XHCI_TRB_CYCLE;
        if (!data_in) {
            data_trb.control |= XHCI_TRB_ISP; /* Immediate data for OUT */
        }
        
        /* Enqueue Data Stage TRB to transfer ring */
        extern int xhci_transfer_ring_enqueue(xhci_transfer_ring_t *ring, xhci_trb_t *trb);
        if (xhci_transfer_ring_enqueue(transfer_ring, &data_trb) < 0) {
            klog_printf(KLOG_ERROR, "xhci: failed to enqueue data TRB");
            transfer->status = USB_TRANSFER_ERROR;
            kfree(state);
            transfer->controller_private = NULL;
            xhci->active_control_transfers[slot - 1] = NULL;
            return -1;
        }
        
        /* Ring doorbell for endpoint 0 */
        extern void xhci_ring_doorbell(xhci_controller_t *xhci, uint8_t slot, uint8_t endpoint, uint16_t stream_id);
        xhci_ring_doorbell(xhci, slot, 0, 0);
        
        /* Wait for Data Stage completion */
        if (xhci_wait_for_transfer_completion(xhci, transfer, 1000, false, true, false) < 0) {
            klog_printf(KLOG_ERROR, "xhci: data stage failed");
            kfree(state);
            transfer->controller_private = NULL;
            xhci->active_control_transfers[slot - 1] = NULL;
            return -1;
        }
    }
    
    /* Step 3: Status Stage TRB */
    xhci_trb_t status_trb;
    k_memset(&status_trb, 0, sizeof(xhci_trb_t));
    
    status_trb.parameter = 0; /* No data */
    status_trb.status = 0 | (XHCI_TRB_IOC << 16);
    
    /* Status stage direction is opposite of data stage */
    uint32_t status_type = XHCI_TRB_TYPE_STATUS_STAGE;
    uint32_t status_trt = data_in ? 1 : 2; /* Opposite of data stage: if data was IN, status is OUT */
    status_trb.control = (status_type << XHCI_TRB_TYPE_SHIFT) | (status_trt << XHCI_TRB_TRT_SHIFT) | XHCI_TRB_CYCLE;
    
    /* Enqueue Status Stage TRB to transfer ring */
    if (xhci_transfer_ring_enqueue(transfer_ring, &status_trb) < 0) {
        klog_printf(KLOG_ERROR, "xhci: failed to enqueue status TRB");
        transfer->status = USB_TRANSFER_ERROR;
        kfree(state);
        transfer->controller_private = NULL;
        xhci->active_control_transfers[slot - 1] = NULL;
        return -1;
    }
    
    /* Ring doorbell for endpoint 0 */
    xhci_ring_doorbell(xhci, slot, 0, 0);
    
    /* Wait for Status Stage completion */
    if (xhci_wait_for_transfer_completion(xhci, transfer, 1000, false, false, true) < 0) {
        klog_printf(KLOG_ERROR, "xhci: status stage failed");
        kfree(state);
        transfer->controller_private = NULL;
        xhci->active_control_transfers[slot - 1] = NULL;
        return -1;
    }
    
    /* All stages completed successfully */
    transfer->status = USB_TRANSFER_SUCCESS;
    klog_printf(KLOG_DEBUG, "xhci: control transfer completed successfully, actual_length=%zu",
                transfer->actual_length);
    
    /* Cleanup */
    kfree(state);
    transfer->controller_private = NULL;
    xhci->active_control_transfers[slot - 1] = NULL;
    
    return 0;
}

/**
 * Process event ring and handle completions
 */
int xhci_process_events(xhci_controller_t *xhci) {
    if (!xhci) return -1;
    
    int events_processed = 0;
    
    /* Process events until ring is empty */
    while (1) {
        xhci_event_trb_t event;
        if (xhci_event_ring_dequeue(&xhci->event_ring, &event, xhci->rt_regs) < 0) {
            break; /* No more events */
        }
        
        events_processed++;
        
        /* Extract fields from event TRB */
        uint8_t completion_code = event.status & XHCI_EVENT_TRB_COMPLETION_CODE_MASK;
        uint32_t transfer_length = (event.status >> XHCI_EVENT_TRB_TRANSFER_LENGTH_SHIFT) & XHCI_EVENT_TRB_TRANSFER_LENGTH_MASK;
        uint8_t slot_id = (event.control >> XHCI_EVENT_TRB_SLOT_ID_SHIFT) & XHCI_EVENT_TRB_SLOT_ID_MASK;
        uint8_t endpoint_id = (event.control >> XHCI_EVENT_TRB_ENDPOINT_ID_SHIFT) & XHCI_EVENT_TRB_ENDPOINT_ID_MASK;
        uint8_t event_type = (event.control >> XHCI_EVENT_TRB_TYPE_SHIFT) & XHCI_EVENT_TRB_TYPE_MASK;
        
        klog_printf(KLOG_DEBUG, "xhci: event slot=%u ep=%u type=%u code=%u length=%u",
                    slot_id, endpoint_id, event_type, completion_code, transfer_length);
        
        /* Handle Transfer Event (type 32) */
        if (event_type == 32) {
            /* Find active control transfer for this slot */
            if (slot_id > 0 && slot_id <= xhci->num_slots) {
                usb_transfer_t *transfer = xhci->active_control_transfers[slot_id - 1];
                if (transfer) {
                    xhci_transfer_state_t *state = (xhci_transfer_state_t *)transfer->controller_private;
                    if (state) {
                        /* Update completion flags based on TRB type */
                        xhci_trb_t *completed_trb = (xhci_trb_t *)(uintptr_t)event.data;
                        uint8_t trb_type = (completed_trb->control >> XHCI_TRB_TYPE_SHIFT) & 0x3F;
                        
                        if (trb_type == XHCI_TRB_TYPE_SETUP_STAGE) {
                            state->setup_complete = true;
                            klog_printf(KLOG_DEBUG, "xhci: setup stage completed");
                        } else if (trb_type == XHCI_TRB_TYPE_DATA_STAGE) {
                            state->data_complete = true;
                            state->data_length = transfer_length;
                            transfer->actual_length = transfer_length;
                            klog_printf(KLOG_DEBUG, "xhci: data stage completed, length=%u", transfer_length);
                        } else if (trb_type == XHCI_TRB_TYPE_STATUS_STAGE) {
                            state->status_complete = true;
                            klog_printf(KLOG_DEBUG, "xhci: status stage completed");
                        }
                        
                        state->completion_code = completion_code;
                        
                        /* Update transfer status */
                        if (completion_code == XHCI_TRB_CC_SUCCESS) {
                            /* Success - status will be set when all stages complete */
                        } else {
                            transfer->status = USB_TRANSFER_ERROR;
                            klog_printf(KLOG_WARN, "xhci: transfer error code=%u", completion_code);
                        }
                    }
                }
            }
        } else if (event_type == 33) { /* Command Completion Event */
            /* Command completion is handled in xhci_device.c via xhci_handle_command_completion */
            /* We need to call it here so it gets processed */
            extern void xhci_handle_command_completion(xhci_controller_t *xhci, xhci_event_trb_t *event);
            xhci_handle_command_completion(xhci, &event);
            klog_printf(KLOG_DEBUG, "xhci: command completion event processed");
        } else {
            klog_printf(KLOG_DEBUG, "xhci: event type=%u code=%u", event_type, completion_code);
        }
        
        /* Note: ERDP and EHB are already updated by xhci_event_ring_dequeue() */
    }
    
    /* CRITICAL: Clear IP (Interrupt Pending) bit in IMAN after processing events
     * IP is a "write 1 to clear" bit - we must clear it so HC can generate new interrupts
     * Without this, the HC may stop generating interrupts
     */
    if (events_processed > 0) {
        uint32_t iman = XHCI_READ32(xhci->rt_regs, XHCI_IMAN(0));
        XHCI_WRITE32(xhci->rt_regs, XHCI_IMAN(0), iman | (1 << 0)); /* Clear IP by writing 1 */
        __asm__ volatile("mfence" ::: "memory");
    }
    
    return events_processed;
}

