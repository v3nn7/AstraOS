#include "../include/usb_device.h"
#include "../include/usb_hub.h"

bool usb_hub_init(usb_hub_t* hub, usb_controller_t* ctrl) {
    if (!hub || !ctrl) {
        return false;
    }
    hub->controller = ctrl;
    hub->port_count = 2;
    for (uint8_t i = 0; i < hub->port_count; ++i) {
        hub->ports[i].connected = false;
        hub->ports[i].speed = USB_SPEED_FULL;
    }
    return true;
}

bool usb_hub_enumerate(usb_hub_t* hub) {
    if (!hub) {
        return false;
    }
    /* Stub: no real devices connected; leave ports disconnected. */
    return true;
}

uint8_t usb_hub_port_count(const usb_hub_t* hub) {
    return hub ? hub->port_count : 0;
}

usb_hub_port_status_t usb_hub_port_status(const usb_hub_t* hub, uint8_t index) {
    usb_hub_port_status_t st = {false, USB_SPEED_FULL};
    if (!hub || index >= hub->port_count) {
        return st;
    }
    return hub->ports[index];
}

bool usb_hub_connect_port(usb_hub_t* hub, uint8_t index, usb_speed_t speed) {
    if (!hub || index >= hub->port_count) {
        return false;
    }
    hub->ports[index].connected = true;
    hub->ports[index].speed = speed;
    return true;
}

bool usb_hub_disconnect_port(usb_hub_t* hub, uint8_t index) {
    if (!hub || index >= hub->port_count) {
        return false;
    }
    hub->ports[index].connected = false;
    return true;
}
