#include "../include/usb_transfer.h"
#include "../include/usb_descriptor.h"

extern void* memset(void* dest, int val, size_t n);

static void fill_device_descriptor(uint8_t* buf, size_t len) {
    if (len < sizeof(usb_device_descriptor_t)) return;
    usb_device_descriptor_t* d = (usb_device_descriptor_t*)buf;
    d->bLength = sizeof(usb_device_descriptor_t);
    d->bDescriptorType = USB_DT_DEVICE;
    d->bcdUSB = 0x0200;
    d->bDeviceClass = 0;
    d->bDeviceSubClass = 0;
    d->bDeviceProtocol = 0;
    d->bMaxPacketSize0 = 64;
    d->idVendor = 0x1234;
    d->idProduct = 0x5678;
    d->bcdDevice = 0x0001;
    d->iManufacturer = 1;
    d->iProduct = 2;
    d->iSerialNumber = 3;
    d->bNumConfigurations = 1;
}

bool usb_submit_control(usb_transfer_t* xfer, const usb_setup_packet_t* setup) {
    if (!xfer || !setup) {
        return false;
    }
    xfer->status = USB_TRANSFER_STATUS_COMPLETED;

    if (setup->bRequest == USB_REQ_GET_DESCRIPTOR) {
        uint8_t dtype = (uint8_t)(setup->wValue >> 8);
        if (dtype == USB_DT_DEVICE && xfer->buffer && xfer->length >= sizeof(usb_device_descriptor_t)) {
            memset(xfer->buffer, 0, xfer->length);
            fill_device_descriptor((uint8_t*)xfer->buffer, xfer->length);
            return true;
        }
    }
    /* SET_ADDRESS / SET_CONFIGURATION succeed trivially in this host model. */
    return true;
}
