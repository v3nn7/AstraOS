#ifndef USB_HUB_H
#define USB_HUB_H

#include <stdint.h>
#include <stdbool.h>
#include "usb_controller.h"
#include "usb_defs.h"

typedef struct usb_hub_port_status {
    bool connected;
    usb_speed_t speed;
} usb_hub_port_status_t;

typedef struct usb_hub {
    usb_controller_t* controller;
    uint8_t port_count;
    usb_hub_port_status_t ports[8];
} usb_hub_t;

/* Initialize a root hub for the given controller (stubbed). */
bool usb_hub_init(usb_hub_t* hub, usb_controller_t* ctrl);

/* Enumerate downstream ports (stubbed to two ports, disconnected). */
bool usb_hub_enumerate(usb_hub_t* hub);

uint8_t usb_hub_port_count(const usb_hub_t* hub);
usb_hub_port_status_t usb_hub_port_status(const usb_hub_t* hub, uint8_t index);
bool usb_hub_connect_port(usb_hub_t* hub, uint8_t index, usb_speed_t speed);
bool usb_hub_disconnect_port(usb_hub_t* hub, uint8_t index);

#endif /* USB_HUB_H */
