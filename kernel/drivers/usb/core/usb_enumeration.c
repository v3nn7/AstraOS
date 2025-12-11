/**
 * USB enumeration stub: invokes usb_device_enumerate when requested.
 */

#include <drivers/usb/usb_device.h>
#include "klog.h"
#include "usb_status.h"

int usb_enumerate_device(usb_device_t *dev) {
    if (!dev) {
        return USB_STATUS_NO_DEVICE;
    }
    int rc = usb_device_enumerate(dev);
    klog_printf(KLOG_INFO, "usb: enumerate addr=%u rc=%d", dev->address, rc);
    return rc;
}
