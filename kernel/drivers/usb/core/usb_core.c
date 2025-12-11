/**
 * USB Core - host/device bookkeeping (stub)
 */

#include <drivers/usb/usb_core.h>
#include "klog.h"
#include "string.h"

static usb_host_controller_t *g_host_controllers = NULL;
static usb_driver_t *g_drivers[16];
static int g_driver_count = 0;

int usb_core_init(void) {
    g_host_controllers = NULL;
    g_driver_count = 0;
    memset(g_drivers, 0, sizeof(g_drivers));
    klog_printf(KLOG_INFO, "usb: core init");
    return 0;
}

void usb_core_cleanup(void) {
    g_host_controllers = NULL;
    g_driver_count = 0;
    klog_printf(KLOG_INFO, "usb: core cleanup");
}

int usb_host_register(usb_host_controller_t *hc) {
    if (!hc) {
        return -1;
    }
    hc->next = g_host_controllers;
    g_host_controllers = hc;
    hc->enabled = true;
    klog_printf(KLOG_INFO, "usb: host register %s type=%d", hc->name ? hc->name : "?", hc->type);
    return 0;
}

int usb_host_unregister(usb_host_controller_t *hc) {
    if (!hc) {
        return -1;
    }
    usb_host_controller_t **cur = &g_host_controllers;
    while (*cur) {
        if (*cur == hc) {
            *cur = hc->next;
            hc->next = NULL;
            klog_printf(KLOG_INFO, "usb: host unregister %s", hc->name ? hc->name : "?");
            return 0;
        }
        cur = &(*cur)->next;
    }
    return -1;
}

usb_host_controller_t *usb_host_find_by_type(usb_controller_type_t type) {
    usb_host_controller_t *cur = g_host_controllers;
    while (cur) {
        if (cur->type == type) {
            return cur;
        }
        cur = cur->next;
    }
    return NULL;
}

int usb_register_driver(usb_driver_t *drv) {
    if (!drv || g_driver_count >= (int)(sizeof(g_drivers) / sizeof(g_drivers[0]))) {
        return -1;
    }
    g_drivers[g_driver_count++] = drv;
    klog_printf(KLOG_INFO, "usb: driver registered %s", drv->name ? drv->name : "?");
    return 0;
}

int usb_bind_driver(usb_device_t *dev) {
    if (!dev) {
        return -1;
    }
    for (int i = 0; i < g_driver_count; i++) {
        usb_driver_t *drv = g_drivers[i];
        if (drv && drv->probe && drv->probe(dev) == 0) {
            if (!drv->init || drv->init(dev) == 0) {
                dev->driver_data = drv;
                klog_printf(KLOG_INFO, "usb: driver %s bound to addr=%u", drv->name ? drv->name : "?", dev->address);
                return 0;
            }
        }
    }
    return -1;
}

int usb_get_driver_count(void) {
    return g_driver_count;
}

usb_driver_t *usb_get_driver(int index) {
    if (index < 0 || index >= g_driver_count) {
        return NULL;
    }
    return g_drivers[index];
}

void usb_debug_print_device(usb_device_t *dev) {
    if (!dev) {
        return;
    }
    klog_printf(KLOG_DEBUG, "usb: dev addr=%u vid=%04x pid=%04x state=%d",
                dev->address, dev->vendor_id, dev->product_id, dev->state);
}

void usb_debug_print_transfer(usb_transfer_t *transfer) {
    if (!transfer) {
        return;
    }
    klog_printf(KLOG_DEBUG, "usb: xfer len=%zu status=%d", transfer->length, transfer->status);
}

/* Helpers to expose host/device counts (for UI/debug). */
int usb_host_count(void) {
    int count = 0;
    usb_host_controller_t *cur = g_host_controllers;
    while (cur) {
        count++;
        cur = cur->next;
    }
    return count;
}
/**
 * USB Core Implementation
 * 
 * Manages USB device tree, enumeration, and provides unified API
 * for all USB host controllers.
 */

#include <drivers/usb/usb_core.h>
#include <drivers/usb/usb_device.h>
#include <drivers/usb/usb_transfer.h>
#include <drivers/usb/usb_descriptors.h>
#include "kmalloc.h"
#include "kernel.h"
#include "klog.h"
#include "string.h"

/* Functions moved to usb_manager.c to avoid duplicate definitions */

