#pragma once

#include <drivers/usb/usb_device.h>
#include <drivers/usb/usb_transfer.h>

typedef struct usb_pipe {
    usb_device_t *device;
    usb_endpoint_t *endpoint;
    bool opened;
} usb_pipe_t;

static inline void usb_pipe_init(usb_pipe_t *pipe, usb_device_t *dev, usb_endpoint_t *ep) {
    if (!pipe) return;
    pipe->device = dev;
    pipe->endpoint = ep;
    pipe->opened = false;
}
