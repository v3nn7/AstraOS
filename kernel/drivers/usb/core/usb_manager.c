/**
 * Legacy manager shims that forward to usb_core.c implementation.
 */

#include <drivers/usb/usb_core.h>
#include "klog.h"

int usb_manager_init(void) {
    klog_printf(KLOG_INFO, "usb: manager init");
    return usb_core_init();
}

int usb_manager_register_host(usb_host_controller_t *hc) {
    return usb_host_register(hc);
}

int usb_manager_unregister_host(usb_host_controller_t *hc) {
    return usb_host_unregister(hc);
}

usb_host_controller_t *usb_manager_find(usb_controller_type_t type) {
    return usb_host_find_by_type(type);
}

__attribute__((unused)) static void usb_manager_selftest(void) {
    /* Placeholder self test */
    klog_printf(KLOG_INFO, "usb: manager selftest");
}
