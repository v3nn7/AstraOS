/**
 * Minimal EHCI stub to avoid crashes.
 */

#include <drivers/usb/usb_core.h>
#include "klog.h"

int ehci_init(usb_host_controller_t *hc) {
    (void)hc;
    klog_printf(KLOG_INFO, "ehci: stub init");
    return 0;
}

void ehci_cleanup(usb_host_controller_t *hc) {
    (void)hc;
    klog_printf(KLOG_INFO, "ehci: stub cleanup");
}
