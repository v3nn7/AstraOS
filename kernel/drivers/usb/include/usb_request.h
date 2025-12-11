#ifndef USB_REQUEST_H
#define USB_REQUEST_H

#include <stdint.h>
#include <stdbool.h>
#include "usb_defs.h"

/* USB setup packet structure used by control transfers. */
typedef struct usb_setup_packet {
    uint8_t bmRequestType;
    uint8_t bRequest;
    uint16_t wValue;
    uint16_t wIndex;
    uint16_t wLength;
} usb_setup_packet_t;

#define USB_REQ_GET_DESCRIPTOR 0x06
#define USB_REQ_SET_ADDRESS    0x05
#define USB_REQ_SET_CONFIGURATION 0x09

#define USB_DT_DEVICE        1
#define USB_DT_CONFIGURATION 2
#define USB_DT_STRING        3
#define USB_DT_INTERFACE     4
#define USB_DT_ENDPOINT      5

/* Helpers to prepare common setup packets. */
void usb_req_build_get_descriptor(usb_setup_packet_t* setup, uint8_t dtype, uint8_t index, uint16_t len);
void usb_req_build_set_address(usb_setup_packet_t* setup, uint8_t address);
void usb_req_build_set_configuration(usb_setup_packet_t* setup, uint8_t config);

#endif /* USB_REQUEST_H */
