/**
 * USB HID Driver Registration
 * 
 * Registers HID keyboard and mouse drivers with USB driver system.
 */

#include <drivers/usb/usb_core.h>
#include <drivers/usb/usb_hid.h>
#include "kmalloc.h"
#include "klog.h"

/* Forward declarations */
extern int usb_hid_keyboard_init(usb_device_t *dev);
extern int usb_hid_mouse_init(usb_device_t *dev);
extern void usb_hid_register_keyboard(usb_device_t *dev);
extern void usb_hid_register_mouse(usb_device_t *dev);

/**
 * Probe for HID keyboard device
 */
static int hid_keyboard_probe(usb_device_t *dev) {
    if (!dev) return -1;
    
    /* HID Keyboard: Class=0x03, Subclass=0x01, Protocol=0x01 */
    if (dev->device_class == 0x03 && 
        dev->device_subclass == 0x01 && 
        dev->device_protocol == 0x01) {
        klog_printf(KLOG_DEBUG, "hid_driver: keyboard probe match");
        return 0;
    }
    
    /* Also match generic HID class if protocol is not set */
    if (dev->device_class == 0x03) {
        /* Check if device has interrupt IN endpoint (keyboard characteristic) */
        for (uint8_t i = 0; i < dev->num_endpoints; i++) {
            usb_endpoint_t *ep = &dev->endpoints[i];
            if ((ep->address & USB_ENDPOINT_DIR_IN) != 0 && 
                ep->type == USB_ENDPOINT_XFER_INT) {
                klog_printf(KLOG_DEBUG, "hid_driver: keyboard probe match (by endpoint)");
                return 0;
            }
        }
    }
    
    return -1;
}

/**
 * Initialize HID keyboard driver
 */
static int hid_keyboard_init(usb_device_t *dev) {
    if (!dev) return -1;
    
    klog_printf(KLOG_INFO, "hid_driver: initializing keyboard driver");
    
    if (usb_hid_keyboard_init(dev) == 0) {
        usb_hid_register_keyboard(dev);
        klog_printf(KLOG_INFO, "hid_driver: keyboard driver initialized");
        return 0;
    }
    
    return -1;
}

/**
 * Probe for HID mouse device
 */
static int hid_mouse_probe(usb_device_t *dev) {
    if (!dev) return -1;
    
    /* HID Mouse: Class=0x03, Subclass=0x01, Protocol=0x02 */
    if (dev->device_class == 0x03 && 
        dev->device_subclass == 0x01 && 
        dev->device_protocol == 0x02) {
        klog_printf(KLOG_DEBUG, "hid_driver: mouse probe match");
        return 0;
    }
    
    return -1;
}

/**
 * Initialize HID mouse driver
 */
static int hid_mouse_init(usb_device_t *dev) {
    if (!dev) return -1;
    
    klog_printf(KLOG_INFO, "hid_driver: initializing mouse driver");
    
    if (usb_hid_mouse_init(dev) == 0) {
        usb_hid_register_mouse(dev);
        klog_printf(KLOG_INFO, "hid_driver: mouse driver initialized");
        return 0;
    }
    
    return -1;
}

/* USB HID Keyboard Driver */
static usb_driver_t usb_hid_keyboard_driver = {
    .name = "usb-hid-keyboard",
    .probe = hid_keyboard_probe,
    .init = hid_keyboard_init,
    .remove = NULL
};

/* USB HID Mouse Driver */
static usb_driver_t usb_hid_mouse_driver = {
    .name = "usb-hid-mouse",
    .probe = hid_mouse_probe,
    .init = hid_mouse_init,
    .remove = NULL
};

/**
 * Register HID drivers
 */
void usb_hid_register_drivers(void) {
    extern int usb_register_driver(usb_driver_t *drv);
    
    usb_register_driver(&usb_hid_keyboard_driver);
    usb_register_driver(&usb_hid_mouse_driver);
    
    klog_printf(KLOG_INFO, "hid_driver: registered HID keyboard and mouse drivers");
}

