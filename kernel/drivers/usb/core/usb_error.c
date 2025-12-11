/**
 * USB error helpers.
 */

#include "usb_status.h"
#include "klog.h"

const char *usb_error_string(int status) {
    return usb_status_to_string(status);
}

void usb_error_log(const char *context, int status) {
    if (!context) {
        context = "usb";
    }
    klog_printf(KLOG_ERROR, "%s: status=%s (%d)", context, usb_status_to_string(status), status);
}
