/**
 * USB Device Implementation
 * 
 * Manages USB device lifecycle, enumeration, and configuration.
 */

#include "usb/usb_device.h"
#include "usb/usb_descriptors.h"
#include "usb/usb_transfer.h"
#include "kmalloc.h"
#include "kernel.h"
#include "klog.h"
#include "string.h"

extern void usb_device_list_add(usb_device_t *dev);
extern void usb_device_list_remove(usb_device_t *dev);
extern uint8_t usb_allocate_device_address(void);

/**
 * Allocate a new USB device structure
 */
usb_device_t *usb_device_alloc(void) {
    usb_device_t *dev = (usb_device_t *)kmalloc(sizeof(usb_device_t));
    if (!dev) {
        klog_printf(KLOG_ERROR, "usb_device: allocation failed");
        return NULL;
    }

    k_memset(dev, 0, sizeof(usb_device_t));
    dev->state = USB_DEVICE_STATE_DEFAULT;
    dev->address = 0;
    dev->speed = USB_SPEED_UNKNOWN;
    dev->num_endpoints = 0;
    dev->num_configurations = 0;
    dev->active_configuration = 0;
    dev->num_children = 0;

    klog_printf(KLOG_DEBUG, "usb_device: allocated device %p", dev);
    return dev;
}

/**
 * Free a USB device structure
 */
void usb_device_free(usb_device_t *dev) {
    if (!dev) return;

    /* Free endpoints */
    for (uint8_t i = 0; i < dev->num_endpoints; i++) {
        if (dev->endpoints[i].controller_private) {
            kfree(dev->endpoints[i].controller_private);
        }
    }

    /* Free descriptors */
    if (dev->descriptors) {
        kfree(dev->descriptors);
    }

    /* Free driver data */
    if (dev->driver_data) {
        kfree(dev->driver_data);
    }

    klog_printf(KLOG_DEBUG, "usb_device: freed device %p", dev);
    kfree(dev);
}

/**
 * Set device address
 */
int usb_device_set_address(usb_device_t *dev, uint8_t address) {
    if (!dev || !dev->controller || !dev->controller->ops) {
        return -1;
    }

    /* Allocate address if needed */
    if (address == 0) {
        address = usb_allocate_device_address();
        if (address == 0) {
            klog_printf(KLOG_ERROR, "usb_device: no free address available");
            return -1;
        }
    }

    /* Send SET_ADDRESS request */
    klog_printf(KLOG_INFO, "usb_device: setting address %d (SET_ADDRESS command)", address);
    int ret = usb_control_transfer(dev, USB_REQ_TYPE_STANDARD | USB_REQ_TYPE_DEVICE,
                                   USB_REQ_SET_ADDRESS, address, 0, NULL, 0, 1000);
    if (ret != 0) {
        klog_printf(KLOG_ERROR, "usb_device: SET_ADDRESS failed (ret=%d)", ret);
        return -1;
    }

    dev->address = address;
    dev->state = USB_DEVICE_STATE_ADDRESS;

    klog_printf(KLOG_INFO, "usb_device: address %d set successfully (device in ADDRESS state)", address);
    klog_printf(KLOG_INFO, "xhci: address device command completed for slot %u -> address %d", dev->slot_id, address);
    return 0;
}

/**
 * Get device descriptor
 */
static int usb_device_get_device_descriptor(usb_device_t *dev) {
    uint8_t buffer[18]; /* Standard device descriptor is 18 bytes */
    
    int ret = usb_get_descriptor(dev, USB_DT_DEVICE, 0, 0, buffer, sizeof(buffer));
    if (ret < 0) {
        klog_printf(KLOG_ERROR, "usb_device: failed to get device descriptor");
        return -1;
    }

    /* Parse device descriptor */
    usb_device_descriptor_t *desc = (usb_device_descriptor_t *)buffer;
    dev->vendor_id = desc->idVendor;
    dev->product_id = desc->idProduct;
    dev->device_class = desc->bDeviceClass;
    dev->device_subclass = desc->bDeviceSubClass;
    dev->device_protocol = desc->bDeviceProtocol;
    dev->num_configurations = desc->bNumConfigurations;

    klog_printf(KLOG_INFO, "usb_device: VID:PID=%04x:%04x Class=%02x:%02x:%02x",
                dev->vendor_id, dev->product_id,
                dev->device_class, dev->device_subclass, dev->device_protocol);

    return 0;
}

/**
 * Set device configuration
 */
int usb_device_set_configuration(usb_device_t *dev, uint8_t config) {
    if (!dev || !dev->controller) {
        return -1;
    }

    if (config > dev->num_configurations) {
        klog_printf(KLOG_ERROR, "usb_device: invalid configuration %d", config);
        return -1;
    }

    /* Send SET_CONFIGURATION request */
    int ret = usb_control_transfer(dev, USB_REQ_TYPE_STANDARD | USB_REQ_TYPE_DEVICE,
                                   USB_REQ_SET_CONFIGURATION, config, 0, NULL, 0, 1000);
    if (ret != 0) {
        klog_printf(KLOG_ERROR, "usb_device: SET_CONFIGURATION failed");
        return -1;
    }

    dev->active_configuration = config;
    dev->state = USB_DEVICE_STATE_CONFIGURED;

    klog_printf(KLOG_INFO, "usb_device: set configuration %d", config);
    return 0;
}

/**
 * Get current configuration
 */
int usb_device_get_configuration(usb_device_t *dev, uint8_t *config) {
    if (!dev || !config) {
        return -1;
    }

    uint8_t buffer[1];
    int ret = usb_control_transfer(dev, USB_REQ_TYPE_STANDARD | USB_REQ_TYPE_DEVICE | USB_ENDPOINT_DIR_IN,
                                   USB_REQ_GET_CONFIGURATION, 0, 0, buffer, 1, 1000);
    if (ret != 1) {
        return -1;
    }

    *config = buffer[0];
    return 0;
}

/**
 * Set device state
 */
int usb_device_set_state(usb_device_t *dev, usb_device_state_t state) {
    if (!dev) return -1;
    dev->state = state;
    return 0;
}

/**
 * Get device state
 */
usb_device_state_t usb_device_get_state(usb_device_t *dev) {
    if (!dev) return USB_DEVICE_STATE_DISCONNECTED;
    return dev->state;
}

/**
 * Add endpoint to device
 */
int usb_device_add_endpoint(usb_device_t *dev, uint8_t address,
                            uint8_t attributes, uint16_t max_packet_size,
                            uint8_t interval) {
    if (!dev) return -1;

    if (dev->num_endpoints >= 32) {
        klog_printf(KLOG_ERROR, "usb_device: too many endpoints");
        return -1;
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
    klog_printf(KLOG_DEBUG, "usb_device: added endpoint 0x%02x (type=%d, max=%d)",
                address, ep->type, max_packet_size);

    return 0;
}

/**
 * Find endpoint by address
 */
usb_endpoint_t *usb_device_find_endpoint(usb_device_t *dev, uint8_t address) {
    if (!dev) return NULL;

    for (uint8_t i = 0; i < dev->num_endpoints; i++) {
        if (dev->endpoints[i].address == address) {
            return &dev->endpoints[i];
        }
    }

    return NULL;
}

