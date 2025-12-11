#ifndef USB_CONTROLLER_H
#define USB_CONTROLLER_H

#include <stdint.h>
#include <stdbool.h>
#include "usb_defs.h"

/* Generic controller descriptor shared by xHCI/EHCI/OHCI backends. */
typedef enum {
    USB_CONTROLLER_XHCI = 0,
    USB_CONTROLLER_EHCI,
    USB_CONTROLLER_OHCI
} usb_controller_type_t;

typedef struct usb_port_status {
    bool connected;
    usb_speed_t negotiated_speed;
    uint8_t port_number;
} usb_port_status_t;

typedef struct usb_controller {
    usb_controller_type_t type;
    uint8_t bus_number;
    uint8_t slot_number;
    usb_speed_t max_speed;
    void *driver_data; /* controller-specific context */
} usb_controller_t;

#endif /* USB_CONTROLLER_H */
