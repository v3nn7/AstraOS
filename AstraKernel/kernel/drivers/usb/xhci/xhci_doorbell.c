/**
 * XHCI Doorbell Registers
 * 
 * Implements doorbell mechanism for notifying XHCI controller
 * about new commands and transfers.
 */

#include "usb/xhci.h"
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
 */
void xhci_ring_cmd_doorbell(xhci_controller_t *xhci) {
    if (!xhci || !xhci->cap_regs) return;
    
    /* Get doorbell base from DBOFF */
    uint32_t dboff = XHCI_READ32(xhci->cap_regs, XHCI_DBOFF) & 0xFFFF;
    void *doorbell_base = (void *)((uintptr_t)xhci->cap_regs + dboff);
    
    /* Ring command doorbell (slot 0) */
    XHCI_WRITE32(doorbell_base, 0, 0); /* Slot 0, Endpoint 0 */
    
    /* Memory barrier to ensure write is visible */
    __asm__ volatile("mfence" ::: "memory");
}

/**
 * Ring doorbell for slot/endpoint
 */
void xhci_ring_doorbell(xhci_controller_t *xhci, uint8_t slot, uint8_t endpoint, uint16_t stream_id) {
    if (!xhci || !xhci->cap_regs || slot == 0 || slot > 255) return;
    
    /* Get doorbell base from DBOFF */
    uint32_t dboff = XHCI_READ32(xhci->cap_regs, XHCI_DBOFF) & 0xFFFF;
    void *doorbell_base = (void *)((uintptr_t)xhci->cap_regs + dboff);
    
    /* Calculate doorbell value */
    uint32_t doorbell_value = ((uint32_t)stream_id << 16) | ((uint32_t)endpoint << 8) | slot;
    
    /* Ring doorbell for this slot */
    XHCI_WRITE32(doorbell_base, slot * 4, doorbell_value);
    
    /* Memory barrier */
    __asm__ volatile("mfence" ::: "memory");
}

