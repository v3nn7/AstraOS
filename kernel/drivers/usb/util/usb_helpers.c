/**
 * USB helper utilities (stub).
 */

#include <drivers/usb/util/usb_helpers.h>
#include <drivers/usb/usb_core.h>
#include "klog.h"

uint8_t usb_crc5(uint16_t data, uint8_t nbits) {
    (void)data;
    (void)nbits;
    return 0;
}

uint16_t usb_crc16(const uint8_t *data, size_t length) {
    (void)data;
    (void)length;
    return 0;
}

const char *usb_speed_string(usb_speed_t speed) {
    switch (speed) {
        case USB_SPEED_LOW: return "low";
        case USB_SPEED_FULL: return "full";
        case USB_SPEED_HIGH: return "high";
        case USB_SPEED_SUPER: return "super";
        case USB_SPEED_SUPER_PLUS: return "super+";
        default: return "unknown";
    }
}

const char *usb_controller_type_string(usb_controller_type_t type) {
    switch (type) {
        case USB_CONTROLLER_UHCI: return "UHCI";
        case USB_CONTROLLER_OHCI: return "OHCI";
        case USB_CONTROLLER_EHCI: return "EHCI";
        case USB_CONTROLLER_XHCI: return "XHCI";
        default: return "Unknown";
    }
}

const char *usb_endpoint_type_string(uint8_t type) {
    switch (type) {
        case USB_ENDPOINT_XFER_CONTROL: return "control";
        case USB_ENDPOINT_XFER_ISOC: return "isoc";
        case USB_ENDPOINT_XFER_BULK: return "bulk";
        case USB_ENDPOINT_XFER_INT: return "interrupt";
        default: return "unknown";
    }
}

uint8_t usb_endpoint_address(uint8_t bEndpointAddress) {
    return bEndpointAddress & 0x0F;
}

uint8_t usb_endpoint_direction(uint8_t bEndpointAddress) {
    return bEndpointAddress & USB_ENDPOINT_DIR_IN;
}

uint8_t usb_endpoint_type(uint8_t bmAttributes) {
    return bmAttributes & 0x3;
}

uint8_t usb_endpoint_sync_type(uint8_t bmAttributes) {
    return (bmAttributes >> 2) & 0x3;
}

uint8_t usb_endpoint_usage_type(uint8_t bmAttributes) {
    return (bmAttributes >> 4) & 0x3;
}

uint16_t usb_frame_from_microframe(uint32_t microframe) {
    return (uint16_t)(microframe / 8);
}

uint8_t usb_microframe_from_frame(uint16_t frame) {
    return (uint8_t)(frame * 8);
}

int usb_wait_for_condition(bool (*condition)(void), uint32_t timeout_us) {
    (void)timeout_us;
    if (!condition) {
        return -1;
    }
    return condition() ? 0 : -1;
}

void usb_dump_descriptor(const uint8_t *data, size_t length) {
    (void)data;
    (void)length;
}
