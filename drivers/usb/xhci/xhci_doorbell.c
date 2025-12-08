/**
 * XHCI Doorbell Registers
 * 
 * Implements doorbell mechanism for notifying XHCI controller
 * about new commands and transfers.
 */

#include "usb/xhci.h"
#include "usb/xhci_internal.h"
#include "mmio.h"
#include "kernel.h"
#include "klog.h"

/* Helper macros */
#define XHCI_READ32(regs, offset) mmio_read32((volatile uint32_t *)((uintptr_t)(regs) + (offset)))
#define XHCI_WRITE32(regs, offset, val) mmio_write32((volatile uint32_t *)((uintptr_t)(regs) + (offset)), val)

/* Doorbell Register Offset */
/* Doorbell registers start at offset from DBOFF in capability registers */
/* Each doorbell is 4 bytes: [31:16] = Stream ID, [15:8] = Endpoint ID, [7:0] = Slot ID */

/**
 * Ring command doorbell (slot 0, endpoint 0)
 * 
 * CRITICAL: Command doorbell is at offset 0 from doorbell_base
 * Value written: 0 (slot=0, endpoint=0, stream=0)
 * This notifies HC that new commands are available in command ring
 */
void xhci_ring_cmd_doorbell(xhci_controller_t *xhci) {
    if (!xhci || !xhci->cap_regs) {
        klog_printf(KLOG_ERROR, "xhci_doorbell: invalid controller");
        return;
    }
    
    /* Use doorbell_regs structure directly */
    klog_printf(KLOG_DEBUG, "xhci_doorbell: ringing command doorbell (DB0)");
    
    /* Ring command doorbell (DB0) */
    /* Doorbell register format: [31:16] = Stream ID, [15:8] = Endpoint ID, [7:0] = Slot ID */
    /* For command ring: slot=0, endpoint=0, stream=0 */
    xhci->doorbell_regs->doorbell[0] = 0;
    
    /* Memory barrier to ensure write is visible to HC */
    __asm__ volatile("mfence" ::: "memory");
    
    klog_printf(KLOG_DEBUG, "xhci_doorbell: command doorbell rung");
}

/**
 * Ring doorbell for slot/endpoint
 */
void xhci_ring_doorbell(xhci_controller_t *xhci, uint8_t slot, uint8_t endpoint, uint16_t stream_id) {
    if (!xhci || !xhci->doorbell_regs || slot == 0) return;
    
    /* Calculate doorbell value */
    uint32_t doorbell_value = ((uint32_t)stream_id << 16) | ((uint32_t)endpoint << 8) | slot;
    
    /* Ring doorbell for this slot using structure */
    xhci->doorbell_regs->doorbell[slot] = doorbell_value;
    
    /* Memory barrier */
    __asm__ volatile("mfence" ::: "memory");
}



