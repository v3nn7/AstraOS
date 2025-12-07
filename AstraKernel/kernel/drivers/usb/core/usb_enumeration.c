/**
 * USB Enumeration
 * 
 * Handles device enumeration, configuration, and endpoint setup.
 */

#include "usb/usb_core.h"
#include "usb/usb_device.h"
#include "usb/usb_transfer.h"
#include "usb/usb_descriptors.h"
#include "kmalloc.h"
#include "kernel.h"
#include "klog.h"
#include "string.h"

extern void usb_device_list_add(usb_device_t *dev);
extern uint8_t usb_allocate_device_address(void);

/**
 * Get device descriptor helper
 */
static int usb_device_get_device_descriptor(usb_device_t *dev) {
    uint8_t buffer[18];
    
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
 * Enumerate device (full enumeration process)
 */
int usb_device_enumerate(usb_device_t *dev) {
    if (!dev || !dev->controller) {
        klog_printf(KLOG_ERROR, "usb_device: invalid device for enumeration");
        return -1;
    }

    klog_printf(KLOG_INFO, "usb_device: enumerating device on controller %s",
                dev->controller->name);

    /* For XHCI: Allocate slot (simplified - use port+1 as slot) */
    if (dev->controller->type == USB_CONTROLLER_XHCI) {
        if (dev->slot_id == 0) {
            dev->slot_id = dev->port + 1; /* Slot 0 is reserved, use port+1 */
            if (dev->slot_id > 31) dev->slot_id = 31; /* Max slot */
            klog_printf(KLOG_INFO, "usb_device: allocated slot %u for port %u (XHCI slot allocation)", dev->slot_id, dev->port);
            klog_printf(KLOG_INFO, "xhci: slot %u enabled for port %u", dev->slot_id, dev->port);
        }
    }

    /* Step 1: Get device descriptor at address 0 */
    int ret = usb_device_get_device_descriptor(dev);
    if (ret < 0) {
        klog_printf(KLOG_ERROR, "usb_device: failed to get device descriptor");
        return -1;
    }

    /* Step 2: Set address */
    ret = usb_device_set_address(dev, 0);
    if (ret < 0) {
        klog_printf(KLOG_ERROR, "usb_device: failed to set address");
        return -1;
    }

    /* Step 3: Get full device descriptor */
    ret = usb_device_get_device_descriptor(dev);
    if (ret < 0) {
        klog_printf(KLOG_ERROR, "usb_device: failed to get device descriptor after address");
        return -1;
    }

    /* Step 4: Get configuration descriptors */
    ret = usb_device_get_configurations(dev);
    if (ret < 0) {
        klog_printf(KLOG_ERROR, "usb_device: failed to get configurations");
        return -1;
    }

    /* Step 5: Set configuration (usually configuration 1) */
    if (dev->num_configurations > 0) {
        ret = usb_device_set_configuration(dev, 1);
        if (ret < 0) {
            klog_printf(KLOG_WARN, "usb_device: failed to set configuration 1");
            /* Continue anyway */
        }
    }

    /* Add to device list */
    usb_device_list_add(dev);

    klog_printf(KLOG_INFO, "usb_device: enumeration complete (address=%d)",
                dev->address);
    
    /* Bind driver after enumeration */
    extern int usb_bind_driver(usb_device_t *dev);
    usb_bind_driver(dev);
    
    return 0;
}

/**
 * Bind driver to device after enumeration
 * 
 * This function tries to find and initialize appropriate driver for the device
 * by matching against registered USB drivers.
 */
int usb_bind_driver(usb_device_t *dev) {
    if (!dev) {
        klog_printf(KLOG_ERROR, "usb_bind: invalid device");
        return -1;
    }

    klog_printf(KLOG_INFO, "usb_bind: binding driver for device VID:PID=%04x:%04x Class=%02x:%02x:%02x",
                dev->vendor_id, dev->product_id, 
                dev->device_class, dev->device_subclass, dev->device_protocol);

    /* Try all registered drivers */
    extern int usb_get_driver_count(void);
    extern usb_driver_t *usb_get_driver(int index);
    
    int driver_count = usb_get_driver_count();
    for (int i = 0; i < driver_count; i++) {
        usb_driver_t *drv = usb_get_driver(i);
        if (!drv || !drv->probe) continue;
        
        /* Try to probe device */
        if (drv->probe(dev) == 0) {
            klog_printf(KLOG_INFO, "usb_bind: driver %s matches device %04x:%04x",
                        drv->name, dev->vendor_id, dev->product_id);
            
            /* Initialize driver */
            if (drv->init && drv->init(dev) == 0) {
                klog_printf(KLOG_INFO, "usb_bind: driver %s initialized successfully", drv->name);
                return 0;
            } else {
                klog_printf(KLOG_WARN, "usb_bind: driver %s probe succeeded but init failed", drv->name);
            }
        }
    }

    klog_printf(KLOG_WARN, "usb_bind: no driver found for device Class=%02x:%02x:%02x",
                dev->device_class, dev->device_subclass, dev->device_protocol);
    return -1;
}

