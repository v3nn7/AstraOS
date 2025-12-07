/**
 * XHCI Interrupt Handler
 * 
 * Handles XHCI controller interrupts (MSI/MSI-X or legacy IRQ).
 */

#include "usb/xhci.h"
#include "usb/usb_core.h"
#include "interrupts.h"
#include "klog.h"
#include "mmio.h"
#include "kernel.h"

/* Helper macros for MMIO access */
#define XHCI_READ32(regs, offset) mmio_read32((volatile uint32_t *)((uintptr_t)(regs) + (offset)))
#define XHCI_WRITE32(regs, offset, val) mmio_write32((volatile uint32_t *)((uintptr_t)(regs) + (offset)), val)
#define XHCI_READ64(regs, offset) mmio_read64((volatile uint64_t *)((uintptr_t)(regs) + (offset)))
#define XHCI_WRITE64(regs, offset, val) mmio_write64((volatile uint64_t *)((uintptr_t)(regs) + (offset)), val)

/* Global XHCI controller pointer for interrupt handler */
static usb_host_controller_t *global_xhci_controller = NULL;

/**
 * Handle XHCI interrupt
 */
static void xhci_handle_irq(usb_host_controller_t *hc) {
    if (!hc) return;
    
    xhci_controller_t *xhci = (xhci_controller_t *)hc->private_data;
    if (!xhci || !xhci->rt_regs) {
        klog_printf(KLOG_WARN, "xhci: interrupt handler called but controller not initialized");
        return;
    }
    
    /* Check if interrupt is pending */
    uint32_t iman = XHCI_READ32(xhci->rt_regs, XHCI_IMAN(0));
    if (!(iman & (1 << 0))) {
        /* No interrupt pending */
        return;
    }
    
    /* Process event ring */
    extern int xhci_process_events(xhci_controller_t *xhci);
    xhci_process_events(xhci);
    
    /* Clear interrupt pending bit */
    XHCI_WRITE32(xhci->rt_regs, XHCI_IMAN(0), iman | (1 << 0));
}

/**
 * XHCI interrupt handler (called from IDT)
 * Must match irq_handler_t signature: void (*)(interrupt_frame_t *)
 */
void xhci_irq_handler(interrupt_frame_t *frame) {
    if (global_xhci_controller) {
        xhci_handle_irq(global_xhci_controller);
    }
}

/**
 * Register XHCI interrupt handler
 */
void xhci_register_irq_handler(usb_host_controller_t *hc, uint8_t vector) {
    if (!hc) {
        klog_printf(KLOG_ERROR, "xhci: cannot register IRQ handler - invalid controller");
        return;
    }
    
    if (vector == 0 || vector >= 256) {
        klog_printf(KLOG_ERROR, "xhci: invalid interrupt vector %u", vector);
        return;
    }
    
    global_xhci_controller = hc;
    
    extern void irq_register(uint8_t vector, irq_handler_t handler);
    klog_printf(KLOG_INFO, "xhci: registering IRQ handler - vector=%u handler=%p", 
                vector, (void *)xhci_irq_handler);
    irq_register(vector, xhci_irq_handler);
    
    klog_printf(KLOG_INFO, "xhci: IRQ handler registered successfully for vector %u", vector);
}

