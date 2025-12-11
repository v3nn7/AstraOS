#ifndef USB_TRANSFER_H
#define USB_TRANSFER_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include "usb_defs.h"
#include "usb_device.h"
#include "usb_request.h"

/* Generic transfer descriptor shared by host controller implementations. */
typedef enum {
    USB_TRANSFER_STATUS_IDLE = 0,
    USB_TRANSFER_STATUS_QUEUED,
    USB_TRANSFER_STATUS_IN_FLIGHT,
    USB_TRANSFER_STATUS_COMPLETED,
    USB_TRANSFER_STATUS_ERROR
} usb_transfer_status_t;

typedef struct usb_transfer {
    usb_device_t *device;
    uint8_t endpoint_number;
    usb_transfer_type_t type;
    usb_direction_t direction;
    void *buffer;
    size_t length;
    usb_transfer_status_t status;
} usb_transfer_t;

/* Submit a control transfer (host-simulated). */
bool usb_submit_control(usb_transfer_t* xfer, const usb_setup_packet_t* setup);

#endif /* USB_TRANSFER_H */
