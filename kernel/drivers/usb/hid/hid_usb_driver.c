/**
 * C shim for HID driver registration (stub).
 */

#include <drivers/usb/usb_hid.h>
#include "klog.h"

int hid_usb_register(void) {
    klog_printf(KLOG_INFO, "hid: register (stub)");
    usb_hid_init();
    return 0;
}

void hid_usb_unregister(void) {
    klog_printf(KLOG_INFO, "hid: unregister (stub)");
}
