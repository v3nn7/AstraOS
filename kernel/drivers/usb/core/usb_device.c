/**
 * USB Device Management - basic stub implementation
 *
 * Provides minimal, host-safe logic to support enumeration and endpoint
 * handling required by the existing tests. This is not a full hardware
 * implementation.
 */

#include <drivers/usb/usb_device.h>
#include <drivers/usb/usb_descriptors.h>
#include "kmalloc.h"
#include "klog.h"
#include "string.h"

/* Simple device list and address allocator */
static usb_device_t *g_device_list = NULL;
static uint8_t g_next_address = 1;

usb_device_t *usb_device_alloc(void) {
    usb_device_t *dev = (usb_device_t *)kmalloc(sizeof(usb_device_t));
    if (!dev) {
        return NULL;
    }
    memset(dev, 0, sizeof(usb_device_t));
    dev->state = USB_DEVICE_STATE_DEFAULT;
    return dev;
}

void usb_device_free(usb_device_t *dev) {
    if (!dev) {
        return;
    }
    kfree(dev);
}

int usb_device_enumerate(usb_device_t *dev) {
    /* Stub enumeration always succeeds for host tests */
    return dev ? 0 : -1;
}

int usb_device_set_address(usb_device_t *dev, uint8_t address) {
    if (!dev) {
        return -1;
    }
    dev->address = address;
    return 0;
}

int usb_device_set_configuration(usb_device_t *dev, uint8_t config) {
    if (!dev) {
        return -1;
    }
    dev->active_configuration = config;
    dev->state = USB_DEVICE_STATE_CONFIGURED;
    return 0;
}

int usb_device_get_configuration(usb_device_t *dev, uint8_t *config) {
    if (!dev || !config) {
        return -1;
    }
    *config = dev->active_configuration;
    return 0;
}

int usb_device_set_state(usb_device_t *dev, usb_device_state_t state) {
    if (!dev) {
        return -1;
    }
    dev->state = state;
    return 0;
}

usb_device_state_t usb_device_get_state(usb_device_t *dev) {
    return dev ? dev->state : USB_DEVICE_STATE_DISCONNECTED;
}

int usb_device_add_endpoint(usb_device_t *dev, uint8_t address,
                            uint8_t attributes, uint16_t max_packet_size,
                            uint8_t interval) {
    if (!dev || dev->num_endpoints >= 32) {
        return -1;
    }

    usb_endpoint_t *ep = &dev->endpoints[dev->num_endpoints++];
    ep->device = dev;
    ep->address = address;
    ep->attributes = attributes;
    ep->max_packet_size = max_packet_size;
    ep->interval = interval;
    ep->type = attributes & 0x3;
    ep->toggle = false;
    ep->controller_private = NULL;
    return 0;
}

usb_endpoint_t *usb_device_find_endpoint(usb_device_t *dev, uint8_t address) {
    if (!dev) {
        return NULL;
    }

    for (uint8_t i = 0; i < dev->num_endpoints; i++) {
        if (dev->endpoints[i].address == address) {
            return &dev->endpoints[i];
        }
    }
    return NULL;
}

void usb_device_list_add(usb_device_t *dev) {
    if (!dev) {
        return;
    }
    dev->next = g_device_list;
    g_device_list = dev;
}

void usb_device_list_remove(usb_device_t *dev) {
    if (!dev) {
        return;
    }

    usb_device_t **cur = &g_device_list;
    while (*cur) {
        if (*cur == dev) {
            *cur = dev->next;
            dev->next = NULL;
            return;
        }
        cur = &(*cur)->next;
    }
}

uint8_t usb_allocate_device_address(void) {
    /* Wrap around at 127 to stay within USB address space */
    if (g_next_address == 0 || g_next_address > 127) {
        g_next_address = 1;
    }
    return g_next_address++;
}

usb_device_t *usb_device_find_by_address(uint8_t address) {
    usb_device_t *cur = g_device_list;
    while (cur) {
        if (cur->address == address) {
            return cur;
        }
        cur = cur->next;
    }
    return NULL;
}

usb_device_t *usb_device_find_by_vid_pid(uint16_t vid, uint16_t pid) {
    usb_device_t *cur = g_device_list;
    while (cur) {
        if (cur->vendor_id == vid && cur->product_id == pid) {
            return cur;
        }
        cur = cur->next;
    }
    return NULL;
}
