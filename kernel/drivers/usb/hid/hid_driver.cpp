/**
 * HID driver glue (stub).
 */

#include <drivers/usb/usb_hid.h>
#include "klog.h"

void hid_driver_init() {
    klog_printf(KLOG_INFO, "hid: driver init (stub)");
    usb_hid_init();
}
