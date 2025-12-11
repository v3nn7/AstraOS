/**
 * USB device tree helper.
 */

#include <drivers/usb/usb_core.hpp>
#include <drivers/usb/usb_device.h>
#include "klog.h"

namespace {

static void dump_device(const usb_device_t *dev, int depth) {
    if (!dev) return;
    for (int i = 0; i < depth; ++i) klog_printf(KLOG_INFO, "  ");
    klog_printf(KLOG_INFO, "dev addr=%u cls=%02x vid=%04x pid=%04x children=%u",
                dev->address, dev->device_class, dev->vendor_id, dev->product_id, dev->num_children);
    for (uint8_t i = 0; i < dev->num_children; ++i) {
        dump_device(dev->children[i], depth + 1);
    }
}

__attribute__((unused)) static void usb_tree_dump() {
    uint32_t dc = usb::device_count();
    for (uint32_t i = 0; i < dc; ++i) {
        dump_device(usb::device_at((int)i), 0);
    }
}

}  // namespace
