/**
 * USB hub handling stubs.
 */

#include <drivers/usb/usb_core.h>
#include "klog.h"

typedef struct {
    uint8_t port_count;
} usb_hub_state_t;

__attribute__((unused)) static int usb_hub_init(usb_device_t *dev) {
    if (!dev) return -1;
    usb_hub_state_t *st = (usb_hub_state_t *)kmalloc(sizeof(usb_hub_state_t));
    if (!st) return -1;
    st->port_count = 1;
    dev->driver_data = st;
    klog_printf(KLOG_INFO, "hub: init ports=%u", st->port_count);
    return 0;
}

__attribute__((unused)) static void usb_hub_poll(usb_device_t *dev) {
    if (!dev || !dev->driver_data) return;
    /* Placeholder: would check hub status change bitmap */
}

__attribute__((unused)) static void usb_hub_cleanup(usb_device_t *dev) {
    if (dev && dev->driver_data) {
        kfree(dev->driver_data);
        dev->driver_data = NULL;
    }
}
