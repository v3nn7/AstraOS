#pragma once

#include "types.h"
#include "interrupts.h"

/**
 * USB Core - Main USB definitions and structures
 * 
 * Provides unified USB device management, enumeration, and transfer scheduling
 * across all controller types (UHCI, OHCI, EHCI, XHCI).
 */

/* USB Specification Constants */
#define USB_VERSION_1_0    0x0100
#define USB_VERSION_1_1    0x0110
#define USB_VERSION_2_0    0x0200
#define USB_VERSION_3_0    0x0300
#define USB_VERSION_3_1    0x0310
#define USB_VERSION_3_2    0x0320

/* USB Request Types */
#define USB_REQ_TYPE_STANDARD    (0 << 5)
#define USB_REQ_TYPE_CLASS       (1 << 5)
#define USB_REQ_TYPE_VENDOR      (2 << 5)
#define USB_REQ_TYPE_RESERVED    (3 << 5)

#define USB_REQ_TYPE_DEVICE      0
#define USB_REQ_TYPE_INTERFACE   1
#define USB_REQ_TYPE_ENDPOINT    2
#define USB_REQ_TYPE_OTHER       3

/* USB Request Codes */
#define USB_REQ_GET_STATUS           0x00
#define USB_REQ_CLEAR_FEATURE        0x01
#define USB_REQ_SET_FEATURE          0x03
#define USB_REQ_SET_ADDRESS          0x05
#define USB_REQ_GET_DESCRIPTOR       0x06
#define USB_REQ_SET_DESCRIPTOR       0x07
#define USB_REQ_GET_CONFIGURATION    0x08
#define USB_REQ_SET_CONFIGURATION    0x09
#define USB_REQ_GET_INTERFACE        0x0A
#define USB_REQ_SET_INTERFACE        0x0B
#define USB_REQ_SYNCH_FRAME          0x0C

/* USB Descriptor Types */
#define USB_DT_DEVICE                0x01
#define USB_DT_CONFIGURATION         0x02
#define USB_DT_STRING                0x03
#define USB_DT_INTERFACE             0x04
#define USB_DT_ENDPOINT              0x05
#define USB_DT_DEVICE_QUALIFIER      0x06
#define USB_DT_OTHER_SPEED_CONFIG   0x07
#define USB_DT_INTERFACE_POWER       0x08
#define USB_DT_OTG                   0x09
#define USB_DT_DEBUG                 0x0A
#define USB_DT_INTERFACE_ASSOC       0x0B
#define USB_DT_BOS                   0x0F
#define USB_DT_DEVICE_CAPABILITY     0x10
#define USB_DT_HID                   0x21
#define USB_DT_HID_REPORT            0x22
#define USB_DT_HID_PHYSICAL          0x23
#define USB_DT_CS_INTERFACE          0x24
#define USB_DT_CS_ENDPOINT           0x25
#define USB_DT_HUB                   0x29

/* USB Endpoint Types */
#define USB_ENDPOINT_XFER_CONTROL    0
#define USB_ENDPOINT_XFER_ISOC       1
#define USB_ENDPOINT_XFER_BULK       2
#define USB_ENDPOINT_XFER_INT        3

/* USB Endpoint Directions */
#define USB_ENDPOINT_DIR_OUT          0
#define USB_ENDPOINT_DIR_IN           0x80

/* USB Transfer Status */
typedef enum {
    USB_TRANSFER_SUCCESS = 0,
    USB_TRANSFER_ERROR,
    USB_TRANSFER_STALL,
    USB_TRANSFER_TIMEOUT,
    USB_TRANSFER_NO_DEVICE,
    USB_TRANSFER_BABBLE,
    USB_TRANSFER_CRC_ERROR,
    USB_TRANSFER_SHORT_PACKET
} usb_transfer_status_t;

/* USB Device Speed */
typedef enum {
    USB_SPEED_UNKNOWN = 0,
    USB_SPEED_LOW,      /* 1.5 Mbps */
    USB_SPEED_FULL,     /* 12 Mbps */
    USB_SPEED_HIGH,     /* 480 Mbps */
    USB_SPEED_SUPER,    /* 5 Gbps */
    USB_SPEED_SUPER_PLUS /* 10 Gbps */
} usb_speed_t;

/* USB Controller Type */
typedef enum {
    USB_CONTROLLER_UHCI = 0,
    USB_CONTROLLER_OHCI,
    USB_CONTROLLER_EHCI,
    USB_CONTROLLER_XHCI
} usb_controller_type_t;

/* Forward declarations */
typedef struct usb_device usb_device_t;
typedef struct usb_host_controller usb_host_controller_t;
typedef struct usb_transfer usb_transfer_t;
typedef struct usb_endpoint usb_endpoint_t;
typedef struct usb_interface usb_interface_t;

/* USB Transfer Callback */
typedef void (*usb_transfer_callback_t)(usb_transfer_t *transfer);

/* USB Device States */
typedef enum {
    USB_DEVICE_STATE_DEFAULT = 0,
    USB_DEVICE_STATE_ADDRESS,
    USB_DEVICE_STATE_CONFIGURED,
    USB_DEVICE_STATE_SUSPENDED,
    USB_DEVICE_STATE_DISCONNECTED
} usb_device_state_t;

