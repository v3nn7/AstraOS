/**
 * High-level USB init shim (legacy C path).
 */

#include <drivers/usb/usb_core.h>
#include <drivers/usb/usb_hid.h>
#include "klog.h"

__attribute__((unused)) static int usb_stack_init(void) {
    klog_printf(KLOG_INFO, "usb: init");
    usb_core_init();
    usb_hid_init();
    return 0;
}
