/**
 * USB Manager - Core USB subsystem management
 * 
 * Manages USB host controllers and device tree.
 */

#include <drivers/usb/usb_core.h>
#include <drivers/usb/usb_device.h>
#include <drivers/usb/usb_transfer.h>
#include "kmalloc.h"
#include "kernel.h"
#include "klog.h"
#include "stdio.h"
#include "string.h"

static usb_host_controller_t *usb_host_controllers = NULL;
static usb_device_t *usb_device_list = NULL;
static uint8_t usb_next_address = 1;
static bool usb_core_initialized = false;

/* USB Driver System */
#define MAX_USB_DRIVERS 16
static usb_driver_t *usb_drivers[MAX_USB_DRIVERS];
static int usb_driver_count = 0;

/**
 * Initialize USB core subsystem
 */
int usb_core_init(void) {
    if (usb_core_initialized) {
        klog_printf(KLOG_WARN, "usb_core: already initialized");
        return 0;
    }

    usb_host_controllers = NULL;
    usb_device_list = NULL;
    usb_next_address = 1;
    usb_core_initialized = true;

    klog_printf(KLOG_INFO, "usb_core: initialized");
    return 0;
}

/**
 * Cleanup USB core subsystem
 */
void usb_core_cleanup(void) {
    /* Cleanup all devices */
    usb_device_t *dev = usb_device_list;
    while (dev) {
        usb_device_t *next = dev->next;
        usb_device_free(dev);
        dev = next;
    }

    /* Cleanup all controllers */
    usb_host_controller_t *hc = usb_host_controllers;
    while (hc) {
        usb_host_controller_t *next = hc->next;
        if (hc->ops && hc->ops->cleanup) {
            hc->ops->cleanup(hc);
        }
        hc = next;
    }

    usb_host_controllers = NULL;
    usb_device_list = NULL;
    usb_core_initialized = false;
    klog_printf(KLOG_INFO, "usb_core: cleaned up");
}

/**
 * Register a USB host controller
 */
int usb_host_register(usb_host_controller_t *hc) {
    if (!hc || !hc->ops) {
        klog_printf(KLOG_ERROR, "usb_core: invalid host controller");
        return -1;
    }

    /* Initialize controller */
    if (hc->ops->init && hc->ops->init(hc) != 0) {
        klog_printf(KLOG_ERROR, "usb_core: failed to initialize controller %s", hc->name);
        return -1;
    }

    /* Add to list */
    hc->next = usb_host_controllers;
    usb_host_controllers = hc;
    hc->enabled = true;

    klog_printf(KLOG_INFO, "usb_core: registered controller %s (type=%d, ports=%d)",
                hc->name, hc->type, hc->num_ports);

    return 0;
}

/**
 * Unregister a USB host controller
 */
int usb_host_unregister(usb_host_controller_t *hc) {
    if (!hc) return -1;

    /* Remove from list */
    usb_host_controller_t **prev = &usb_host_controllers;
    while (*prev) {
        if (*prev == hc) {
            *prev = hc->next;
            break;
        }
        prev = &(*prev)->next;
    }

    /* Cleanup */
    if (hc->ops && hc->ops->cleanup) {
        hc->ops->cleanup(hc);
    }

    klog_printf(KLOG_INFO, "usb_core: unregistered controller %s", hc->name);
    return 0;
}

/**
 * Find host controller by type
 */
usb_host_controller_t *usb_host_find_by_type(usb_controller_type_t type) {
    usb_host_controller_t *hc = usb_host_controllers;
    while (hc) {
        if (hc->type == type && hc->enabled) {
            return hc;
        }
        hc = hc->next;
    }
    return NULL;
}

/**
 * Find device by address
 */
usb_device_t *usb_device_find_by_address(uint8_t address) {
    usb_device_t *dev = usb_device_list;
    while (dev) {
        if (dev->address == address) {
            return dev;
        }
        dev = dev->next;
    }
    return NULL;
}

/**
 * Find device by vendor/product ID
 */
usb_device_t *usb_device_find_by_vid_pid(uint16_t vid, uint16_t pid) {
    usb_device_t *dev = usb_device_list;
    while (dev) {
        if (dev->vendor_id == vid && dev->product_id == pid) {
            return dev;
        }
        dev = dev->next;
    }
    return NULL;
}

/**
 * Allocate next available USB address
 */
static uint8_t usb_allocate_address(void) {
    for (uint8_t addr = 1; addr < 128; addr++) {
        if (!usb_device_find_by_address(addr)) {
            return addr;
        }
    }
    return 0;
}

/**
 * Add device to device list
 */
static void usb_device_add(usb_device_t *dev) {
    dev->next = usb_device_list;
    usb_device_list = dev;
}

/**
 * Remove device from device list
 */
static void usb_device_remove(usb_device_t *dev) {
    usb_device_t **prev = &usb_device_list;
    while (*prev) {
        if (*prev == dev) {
            *prev = dev->next;
            break;
        }
        prev = &(*prev)->next;
    }
}

/**
 * Debug: Print device information
 */
void usb_debug_print_device(usb_device_t *dev) {
    if (!dev) {
        printf("usb_debug: device is NULL\n");
        return;
    }

    printf("USB Device:\n");
    printf("  Address: %d\n", dev->address);
    printf("  Speed: %d\n", dev->speed);
    printf("  State: %d\n", dev->state);
    printf("  VID:PID = %04x:%04x\n", dev->vendor_id, dev->product_id);
    printf("  Class:Subclass:Protocol = %02x:%02x:%02x\n",
           dev->device_class, dev->device_subclass, dev->device_protocol);
    printf("  Configurations: %d\n", dev->num_configurations);
    printf("  Active Configuration: %d\n", dev->active_configuration);
    printf("  Endpoints: %d\n", dev->num_endpoints);
    printf("  Controller: %s\n", dev->controller ? dev->controller->name : "NULL");
}

/**
 * Debug: Print transfer information
 */
void usb_debug_print_transfer(usb_transfer_t *transfer) {
    if (!transfer) {
        printf("usb_debug: transfer is NULL\n");
        return;
    }

    printf("USB Transfer:\n");
    printf("  Device: %p\n", transfer->device);
    printf("  Endpoint: 0x%02x\n", transfer->endpoint ? transfer->endpoint->address : 0);
    printf("  Length: %zu\n", transfer->length);
    printf("  Actual Length: %zu\n", transfer->actual_length);
    printf("  Status: %d\n", transfer->status);
    printf("  Timeout: %u ms\n", transfer->timeout_ms);
}

/* Endpoint allocation */
usb_endpoint_t *usb_endpoint_alloc(usb_device_t *dev, uint8_t address,
                                    uint8_t attributes, uint16_t max_packet_size,
                                    uint8_t interval) {
    if (!dev) {
        return NULL;
    }

    if (dev->num_endpoints >= 32) {
        klog_printf(KLOG_ERROR, "usb_core: too many endpoints");
        return NULL;
    }

    usb_endpoint_t *ep = &dev->endpoints[dev->num_endpoints];
    ep->device = dev;
    ep->address = address;
    ep->attributes = attributes;
    ep->max_packet_size = max_packet_size;
    ep->interval = interval;
    ep->type = attributes & 0x03;
    ep->toggle = false;
    ep->controller_private = NULL;

    dev->num_endpoints++;
    return ep;
}

void usb_endpoint_free(usb_endpoint_t *ep) {
    if (!ep) return;
    if (ep->controller_private) {
        kfree(ep->controller_private);
        ep->controller_private = NULL;
    }
}

/* Export device list management functions */
void usb_device_list_add(usb_device_t *dev) {
    usb_device_add(dev);
}

void usb_device_list_remove(usb_device_t *dev) {
    usb_device_remove(dev);
}

uint8_t usb_allocate_device_address(void) {
    return usb_allocate_address();
}

/**
 * Register a USB driver
 */
int usb_register_driver(usb_driver_t *drv) {
    if (!drv || !drv->name) {
        klog_printf(KLOG_ERROR, "usb: invalid driver");
        return -1;
    }
    
    if (usb_driver_count >= MAX_USB_DRIVERS) {
        klog_printf(KLOG_ERROR, "usb: too many drivers (max=%d)", MAX_USB_DRIVERS);
        return -1;
    }
    
    usb_drivers[usb_driver_count++] = drv;
    klog_printf(KLOG_INFO, "usb: registered driver %s", drv->name);
    return 0;
}

/**
 * Get driver count (for external access)
 */
int usb_get_driver_count(void) {
    return usb_driver_count;
}

/**
 * Get driver by index (for external access)
 */
usb_driver_t *usb_get_driver(int index) {
    if (index < 0 || index >= usb_driver_count) {
        return NULL;
    }
    return usb_drivers[index];
}
