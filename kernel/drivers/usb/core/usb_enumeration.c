/**
 * USB enumeration: basic control sequence for device 0 -> configured.
 */

#include <drivers/usb/usb_device.h>
#include <drivers/usb/usb_core.h>
#include <drivers/usb/usb_transfer.h>
#include <drivers/usb/usb_descriptors.h>
#include "klog.h"
#include "usb_status.h"
#include "string.h"

static int get_descriptor(usb_device_t *dev, uint8_t type, uint8_t idx, void *buf, size_t len) {
    return usb_control_transfer(dev, 0x80, USB_REQ_GET_DESCRIPTOR, (type << 8) | idx, 0, buf, (uint16_t)len, 100);
}

int usb_enumerate_device(usb_device_t *dev) {
    if (!dev) return USB_STATUS_NO_DEVICE;

    /* Step 1: short device descriptor */
    usb_device_descriptor_t dd;
    memset(&dd, 0, sizeof(dd));
    if (get_descriptor(dev, USB_DT_DEVICE, 0, &dd, sizeof(dd)) < 0) {
        return USB_STATUS_ERROR;
    }

    /* Step 2: set address */
    uint8_t addr = usb_allocate_device_address();
    if (usb_control_transfer(dev, 0x00, USB_REQ_SET_ADDRESS, addr, 0, NULL, 0, 100) < 0) {
        return USB_STATUS_ERROR;
    }
    usb_device_set_address(dev, addr);
    dev->state = USB_DEVICE_STATE_ADDRESS;

    /* Step 3: full device descriptor */
    if (get_descriptor(dev, USB_DT_DEVICE, 0, &dd, sizeof(dd)) >= 0) {
        dev->vendor_id = dd.idVendor;
        dev->product_id = dd.idProduct;
        dev->device_class = dd.bDeviceClass;
        dev->device_subclass = dd.bDeviceSubClass;
        dev->device_protocol = dd.bDeviceProtocol;
        dev->num_configurations = dd.bNumConfigurations;
    }

    /* Step 4: configuration descriptor (first one) */
    uint8_t cfgbuf[256];
    memset(cfgbuf, 0, sizeof(cfgbuf));
    if (get_descriptor(dev, USB_DT_CONFIGURATION, 0, cfgbuf, sizeof(cfgbuf)) < 0) {
        return USB_STATUS_ERROR;
    }

    /* Step 5: set configuration 1 */
    if (usb_control_transfer(dev, 0x00, USB_REQ_SET_CONFIGURATION, 1, 0, NULL, 0, 100) < 0) {
        return USB_STATUS_ERROR;
    }
    usb_device_set_configuration(dev, 1);
    dev->state = USB_DEVICE_STATE_CONFIGURED;
    klog_printf(KLOG_INFO, "usb: enumerated addr=%u vid=%04x pid=%04x", dev->address, dev->vendor_id, dev->product_id);
    /* Bind first matching driver (e.g. HID) */
    usb_bind_driver(dev);
    return USB_STATUS_OK;
}
