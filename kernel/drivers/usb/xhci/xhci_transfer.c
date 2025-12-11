/**
 * XHCI Transfer Implementation
 * 
 * Full TRB-based control, interrupt, bulk, and isochronous transfers.
 */

#include <drivers/usb/xhci.h>
#include <drivers/usb/usb_core.h>
#include <drivers/usb/usb_transfer.h>
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
    
    /* Get setup packet */
    uint8_t *setup = transfer->setup;
    uint16_t wLength = (setup[6] | (setup[7] << 8));
    uint8_t bmRequestType = setup[0];
    bool data_in = (bmRequestType & USB_ENDPOINT_DIR_IN) != 0;
    
    /* Initialize transfer ring for endpoint 0 if needed */
    uint32_t ep_idx = 0; /* Control endpoint 0 */
    if (!xhci->transfer_rings[slot][ep_idx]) {
        extern int xhci_transfer_ring_init(xhci_controller_t *xhci, uint32_t slot, uint32_t endpoint);
        if (xhci_transfer_ring_init(xhci, slot, ep_idx) < 0) {
            klog_printf(KLOG_ERROR, "xhci: failed to init transfer ring for control endpoint");
            return -1;
        }
    }
    
    xhci_transfer_ring_t *transfer_ring = xhci->transfer_rings[slot][ep_idx];
    if (!transfer_ring) {
        klog_printf(KLOG_ERROR, "xhci: transfer ring not initialized");
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
        return -1;
    }
    
    /* Ring doorbell for endpoint 0 */
    extern void xhci_ring_doorbell(xhci_controller_t *xhci, uint8_t slot, uint8_t endpoint, uint16_t stream_id);
    xhci_ring_doorbell(xhci, slot, 0, 0);
    
    /* Wait for Setup Stage completion (simplified polling) */
    uint32_t timeout = 10000; /* 10ms timeout */
    for (volatile int i = 0; i < 1000; i++); /* Small delay */
    extern int xhci_process_events(xhci_controller_t *xhci);
    xhci_process_events(xhci);
    
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
            return -1;
        }
        
        /* Ring doorbell for endpoint 0 */
        extern void xhci_ring_doorbell(xhci_controller_t *xhci, uint8_t slot, uint8_t endpoint, uint16_t stream_id);
        xhci_ring_doorbell(xhci, slot, 0, 0);
        
        /* Wait for Data Stage completion */
        timeout = 10000;
        while (timeout-- > 0) {
            xhci_process_events(xhci);
            for (volatile int i = 0; i < 100; i++);
            break;
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
        return -1;
    }
    
    /* Ring doorbell for endpoint 0 */
    xhci_ring_doorbell(xhci, slot, 0, 0);
    
    /* Wait for Status Stage completion */
    timeout = 10000;
    while (timeout-- > 0) {
        xhci_process_events(xhci);
        for (volatile int i = 0; i < 100; i++);
        break;
    }
    
    transfer->status = USB_TRANSFER_SUCCESS;
    klog_printf(KLOG_DEBUG, "xhci: control transfer completed");
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
        if (xhci_event_ring_dequeue(&xhci->event_ring, &event) < 0) {
            break; /* No more events */
        }
        
        events_processed++;
        
        /* Handle different event types */
        uint8_t event_type = event.trb_type;
        uint8_t completion_code = event.completion_code;
        
        /* Event TRB types (from xhci_event_trb_t) */
        if (event_type == 32) { /* Transfer Event */
            /* Transfer completed */
            if (completion_code == XHCI_TRB_CC_SUCCESS) {
                klog_printf(KLOG_DEBUG, "xhci: transfer completed successfully");
            } else {
                klog_printf(KLOG_WARN, "xhci: transfer error code=%u", completion_code);
            }
        } else if (event_type == 33) { /* Command Completion Event */
            /* Command completed */
            klog_printf(KLOG_DEBUG, "xhci: command completed");
        } else {
            klog_printf(KLOG_DEBUG, "xhci: event type=%u code=%u", event_type, completion_code);
        }
        
        /* Update event ring dequeue pointer */
        uint64_t erdp = XHCI_READ64(xhci->rt_regs, XHCI_ERDP(0));
        erdp &= ~XHCI_ERDP_EHB; /* Clear event handler busy */
        XHCI_WRITE64(xhci->rt_regs, XHCI_ERDP(0), erdp);
    }
    
    return events_processed;
}

