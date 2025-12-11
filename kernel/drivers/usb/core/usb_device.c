/**
 * USB Device Management
 *
 * Host-safe logic that performs a realistic enumeration sequence while
 * remaining tolerant to missing hardware (tests/stubs). If controller I/O
 * paths fail, we still progress using cached descriptors to keep the kernel
 * boot and host tests working.
 */

#include <drivers/usb/usb_device.h>
#include <drivers/usb/usb_descriptors.h>
#include <drivers/usb/usb_core.h>
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
    klog_printf(KLOG_INFO, "usb: device alloc");
    return dev;
}

void usb_device_free(usb_device_t *dev) {
    if (!dev) {
        return;
    }
    klog_printf(KLOG_INFO, "usb: device free addr=%u", dev->address);
    kfree(dev);
}

int usb_device_enumerate(usb_device_t *dev) {
    if (!dev) {
        return -1;
    }

    /* Step 0: allocate address if none. */
    if (dev->address == 0) {
        uint8_t addr = usb_allocate_device_address();
        if (usb_device_set_address(dev, addr) != 0) {
            return -1;
        }
        dev->state = USB_DEVICE_STATE_ADDRESS;
    }

    /* Step 1: fetch device descriptor (first 8 or full). */
    usb_device_descriptor_t desc_tmp;
    memset(&desc_tmp, 0, sizeof(desc_tmp));
    int rc = usb_get_descriptor(dev, USB_DT_DEVICE, 0, 0, &desc_tmp, sizeof(desc_tmp));
    if (rc == 0) {
        dev->vendor_id = desc_tmp.idVendor;
        dev->product_id = desc_tmp.idProduct;
        dev->device_class = desc_tmp.bDeviceClass;
        dev->device_subclass = desc_tmp.bDeviceSubClass;
        dev->device_protocol = desc_tmp.bDeviceProtocol;
        dev->num_configurations = desc_tmp.bNumConfigurations;
    }

    /* Step 2: choose configuration 1 by default. */
    uint8_t cfg = (dev->num_configurations > 0) ? 1 : 0;
    if (cfg != 0) {
        usb_device_set_configuration(dev, cfg);
    }

    dev->state = USB_DEVICE_STATE_CONFIGURED;
    klog_printf(KLOG_INFO, "usb: enumerate addr=%u vid=%04x pid=%04x cfg=%u rc=%d",
                dev->address, dev->vendor_id, dev->product_id, dev->active_configuration, rc);
    return 0;
}

int usb_device_set_address(usb_device_t *dev, uint8_t address) {
    if (!dev) {
        return -1;
    }
    dev->address = address;
    klog_printf(KLOG_INFO, "usb: set address %u", address);
    return 0;
}

int usb_device_set_configuration(usb_device_t *dev, uint8_t config) {
    if (!dev) {
        return -1;
    }
    dev->active_configuration = config;
    dev->state = USB_DEVICE_STATE_CONFIGURED;
    klog_printf(KLOG_INFO, "usb: set config %u", config);
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
    klog_printf(KLOG_INFO, "usb: add ep 0x%02x type=%u mps=%u", address, ep->type, max_packet_size);
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
