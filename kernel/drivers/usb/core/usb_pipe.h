#pragma once

#include <drivers/usb/usb_device.h>
#include <drivers/usb/usb_transfer.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct usb_pipe {
    usb_device_t *device;
    usb_endpoint_t *endpoint;
    bool opened;
} usb_pipe_t;

void usb_pipe_init(usb_pipe_t *pipe, usb_device_t *dev, usb_endpoint_t *ep);
int usb_pipe_open(usb_pipe_t *pipe);
int usb_pipe_close(usb_pipe_t *pipe);
int usb_pipe_transfer(usb_pipe_t *pipe, void *buf, size_t len, uint32_t timeout_ms);

#ifdef __cplusplus
}
#endif
