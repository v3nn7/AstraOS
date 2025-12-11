/**
 * USB power management stubs.
 */

#include "klog.h"

void usb_power_suspend(void) {
    klog_printf(KLOG_INFO, "usb: suspend (stub)");
}

void usb_power_resume(void) {
    klog_printf(KLOG_INFO, "usb: resume (stub)");
}

void usb_power_port_reset(uint8_t port) {
    klog_printf(KLOG_INFO, "usb: port %u reset (stub)", port);
}
