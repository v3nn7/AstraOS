#include "../include/usb_device.h"

void usb_device_set_descriptor(usb_device_t* dev, const usb_device_descriptor_t* desc) {
    if (!dev || !desc) {
        return;
    }
    dev->descriptor = *desc;
}

const usb_device_descriptor_t* usb_device_get_descriptor(const usb_device_t* dev) {
    if (!dev) {
        return 0;
    }
    return &dev->descriptor;
}

void usb_device_set_address(usb_device_t* dev, uint8_t address) {
    if (!dev) return;
    dev->address = address;
}

void usb_device_set_configuration(usb_device_t* dev, uint8_t cfg) {
    if (!dev) return;
    dev->configuration_value = cfg;
}
