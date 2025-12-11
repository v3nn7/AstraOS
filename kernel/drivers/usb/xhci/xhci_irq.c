/**
 * xHCI IRQ stubs.
 */

#include <drivers/usb/xhci.h>
#include "klog.h"
#include "interrupts.h"

void xhci_register_irq_handler(usb_host_controller_t *hc, uint8_t vector) {
    /* In a full kernel we'd register IDT; here we just log. */
    klog_printf(KLOG_INFO, "xhci: irq handler vector=0x%02x", vector);
    (void)hc;
}

/* Simple poll-based IRQ service */
void xhci_irq_service(xhci_controller_t *ctrl) {
    if (!ctrl) return;
    xhci_process_events(ctrl);
}
