#pragma once
#include "types.h"

#define USB_SPEED_FULL   1
#define USB_SPEED_LOW    2
#define USB_SPEED_HIGH   3
#define USB_SPEED_SUPER  4

typedef struct usb_device usb_device_t;

typedef struct {
    uint8_t bus;
    uint8_t slot;
    uint8_t func;
    uint8_t type; // EHCI=2, XHCI=3
    uint64_t mmio_base;
} usb_controller_t;

typedef struct usb_device {
    usb_controller_t *ctrl;
    uint8_t slot_id;
    uint8_t speed;
    uint8_t address;
} usb_device_t;

void usb_core_init(void);
void usb_register_controller(usb_controller_t c);

usb_device_t* usb_enumerate_xhci(usb_controller_t *ctrl);
