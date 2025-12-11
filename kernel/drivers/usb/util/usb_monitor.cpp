/**
 * Simple USB monitor utilities: dump controllers and devices.
 */

#include <drivers/usb/usb_core.hpp>
#include <drivers/usb/usb_device.h>
#include "klog.h"
#include <string>

namespace {

__attribute__((unused)) static void usb_monitor_dump() {
    uint32_t cc = usb::controller_count();
    uint32_t dc = usb::device_count();
    klog_printf(KLOG_INFO, "usbmon: controllers=%u devices=%u", cc, dc);
    for (uint32_t i = 0; i < cc; ++i) {
        const usb_controller_t *c = usb::controller_at((int)i);
        if (!c) continue;
        klog_printf(KLOG_INFO, "usbmon: ctrl[%u] type=%u name=%s irq=%u", i, c->type, c->name, c->irq);
    }
    for (uint32_t i = 0; i < dc; ++i) {
        const usb_device_t *d = usb::device_at((int)i);
        if (!d) continue;
        klog_printf(KLOG_INFO, "usbmon: dev[%u] addr=%u vid=%04x pid=%04x class=%02x",
                    i, d->address, d->vendor_id, d->product_id, d->device_class);
    }
}

}  // namespace
