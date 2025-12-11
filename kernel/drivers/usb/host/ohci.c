/**
 * Minimal OHCI host controller helpers.
 */

#include <drivers/usb/usb_core.h>
#include "klog.h"

__attribute__((unused)) static void ohci_reset(usb_host_controller_t *hc) {
    if (!hc) return;
    klog_printf(KLOG_INFO, "ohci: reset (stub) base=%p", hc->regs_base);
}

int ohci_init(usb_host_controller_t *hc) {
    if (!hc) return -1;
    klog_printf(KLOG_INFO, "ohci: init base=%p", hc->regs_base);
    ohci_reset(hc);
    hc->enabled = true;
    return 0;
}

void ohci_cleanup(usb_host_controller_t *hc) {
    klog_printf(KLOG_INFO, "ohci: cleanup");
    (void)hc;
}
