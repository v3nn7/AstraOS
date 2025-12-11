#ifndef USB_DEVICE_H
#define USB_DEVICE_H

#include <stdint.h>
#include "usb_defs.h"
#include "usb_controller.h"
#include "usb_descriptor.h"

/* Basic USB device representation bound to a detected controller/port. */
typedef struct usb_device {
    usb_controller_t *controller;
    usb_speed_t speed;
    uint8_t address;
    uint8_t port;
    uint8_t configuration_value;
    uint8_t interface_count;
    usb_device_descriptor_t descriptor;
} usb_device_t;

/* Device helpers. */
void usb_device_set_descriptor(usb_device_t* dev, const usb_device_descriptor_t* desc);
const usb_device_descriptor_t* usb_device_get_descriptor(const usb_device_t* dev);
void usb_device_set_address(usb_device_t* dev, uint8_t address);
void usb_device_set_configuration(usb_device_t* dev, uint8_t cfg);

#endif /* USB_DEVICE_H */
