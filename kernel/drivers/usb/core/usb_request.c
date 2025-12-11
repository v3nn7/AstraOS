#include "../include/usb_request.h"

void usb_req_build_get_descriptor(usb_setup_packet_t* setup, uint8_t dtype, uint8_t index, uint16_t len) {
    if (!setup) return;
    setup->bmRequestType = 0x80;  // device-to-host, standard, device
    setup->bRequest = USB_REQ_GET_DESCRIPTOR;
    setup->wValue = (uint16_t)((dtype << 8) | index);
    setup->wIndex = 0;
    setup->wLength = len;
}

void usb_req_build_set_address(usb_setup_packet_t* setup, uint8_t address) {
    if (!setup) return;
    setup->bmRequestType = 0x00;  // host-to-device, standard, device
    setup->bRequest = USB_REQ_SET_ADDRESS;
    setup->wValue = address;
    setup->wIndex = 0;
    setup->wLength = 0;
}

void usb_req_build_set_configuration(usb_setup_packet_t* setup, uint8_t config) {
    if (!setup) return;
    setup->bmRequestType = 0x00;  // host-to-device, standard, device
    setup->bRequest = USB_REQ_SET_CONFIGURATION;
    setup->wValue = config;
    setup->wIndex = 0;
    setup->wLength = 0;
}
