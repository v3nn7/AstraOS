#ifndef USB_DEFS_H
#define USB_DEFS_H

#include <stdint.h>
#include <stdbool.h>

/* Common USB constants and lightweight enums shared by the driver stack. */
typedef enum {
    USB_SPEED_LOW = 0,
    USB_SPEED_FULL,
    USB_SPEED_HIGH,
    USB_SPEED_SUPER,
    USB_SPEED_SUPER_PLUS
} usb_speed_t;

typedef enum {
    USB_DIR_OUT = 0,
    USB_DIR_IN  = 1
} usb_direction_t;

typedef enum {
    USB_XFER_CONTROL = 0,
    USB_XFER_ISOCHRONOUS,
    USB_XFER_BULK,
    USB_XFER_INTERRUPT
} usb_transfer_type_t;

typedef enum {
    USB_REQ_TYPE_STANDARD = 0,
    USB_REQ_TYPE_CLASS    = 1,
    USB_REQ_TYPE_VENDOR   = 2
} usb_request_type_t;

typedef enum {
    USB_RECIPIENT_DEVICE    = 0,
    USB_RECIPIENT_INTERFACE = 1,
    USB_RECIPIENT_ENDPOINT  = 2,
    USB_RECIPIENT_OTHER     = 3
} usb_request_recipient_t;

#endif /* USB_DEFS_H */
