#pragma once

#include "types.h"

typedef enum {
    USB_CTRL_NONE = 0,
    USB_CTRL_EHCI,
    USB_CTRL_XHCI
} usb_ctrl_type_t;

typedef struct {
    usb_ctrl_type_t type;
    uint8_t bus, slot, func;
    uint16_t vendor, device;
} usb_controller_t;

void usb_init(void);
int usb_controller_count(void);

