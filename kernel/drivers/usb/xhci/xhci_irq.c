/**
 * xHCI IRQ stubs.
 */

#include <drivers/usb/xhci.h>

void xhci_register_irq_handler(usb_host_controller_t *hc, uint8_t vector) {
    (void)hc;
    (void)vector;
}
