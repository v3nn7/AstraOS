#pragma once

/* Simple USB status codes used across core helpers. */
#define USB_STATUS_OK           0
#define USB_STATUS_ERROR        -1
#define USB_STATUS_TIMEOUT      -2
#define USB_STATUS_STALL        -3
#define USB_STATUS_NO_DEVICE    -4
#define USB_STATUS_NO_MEMORY    -5
#define USB_STATUS_IO_ERROR     -6

static inline const char *usb_status_to_string(int status) {
    switch (status) {
    case USB_STATUS_OK: return "ok";
    case USB_STATUS_ERROR: return "error";
    case USB_STATUS_TIMEOUT: return "timeout";
    case USB_STATUS_STALL: return "stall";
    case USB_STATUS_NO_DEVICE: return "no_device";
    case USB_STATUS_NO_MEMORY: return "no_memory";
    case USB_STATUS_IO_ERROR: return "io_error";
    default: return "unknown";
    }
}
