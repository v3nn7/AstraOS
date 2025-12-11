/**
 * xHCI IRQ stubs.
 */

#include <drivers/usb/xhci.h>
#include <drivers/usb/usb_core.h>
#include <drivers/apic/lapic.h>
#include "klog.h"
#include "interrupts.h"

void xhci_register_irq_handler(usb_host_controller_t *hc, uint8_t vector) {
    /* Register simple handler in IDT for legacy IRQ; MSI path is handled in PCI layer */
    (void)hc;
    irq_register_handler(vector, xhci_irq_handler);
    klog_printf(KLOG_INFO, "xhci: irq handler registered vector=0x%02x", vector);
}

/* Simple poll-based IRQ service */
void xhci_irq_service(xhci_controller_t *ctrl) {
    if (!ctrl) return;
    xhci_process_events(ctrl);
}

void xhci_irq_handler(struct interrupt_frame *frame) {
    (void)frame;
    klog_printf(KLOG_INFO, "xhci: irq fired");
    /* Walk controllers? For now assume single xHCI registered as first host */
    usb_host_controller_t *hc = usb_host_find_by_type(USB_CONTROLLER_XHCI);
    if (hc && hc->private_data) {
        xhci_irq_service((xhci_controller_t *)hc->private_data);
    }
    lapic_eoi();
}
