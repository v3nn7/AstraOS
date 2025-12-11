/**
 * USB pipe helpers: lightweight open/submit/close wrappers around transfers.
 */

#include "usb_pipe.h"
#include "usb_status.h"
#include "klog.h"
#include "string.h"

static int usb_pipe_open(usb_pipe_t *pipe) {
    if (!pipe || !pipe->device || !pipe->endpoint) {
        return USB_STATUS_ERROR;
    }
    pipe->opened = true;
    klog_printf(KLOG_INFO, "usb: pipe open ep=0x%02x", pipe->endpoint->address);
    return USB_STATUS_OK;
}

static int usb_pipe_close(usb_pipe_t *pipe) {
    if (!pipe) {
        return USB_STATUS_ERROR;
    }
    pipe->opened = false;
    return USB_STATUS_OK;
}

static int usb_pipe_transfer(usb_pipe_t *pipe, void *buf, size_t len, uint32_t timeout_ms) {
    if (!pipe || !pipe->opened || !pipe->device || !pipe->endpoint) {
        return USB_STATUS_ERROR;
    }
    usb_transfer_t *t = usb_transfer_alloc(pipe->device, pipe->endpoint, len);
    if (!t) {
        return USB_STATUS_NO_MEMORY;
    }
    t->timeout_ms = timeout_ms;
    if (buf && len > 0) {
        memcpy(t->buffer, buf, len < t->length ? len : t->length);
    }
    int rc = usb_transfer_submit(t);
    usb_transfer_free(t);
    return rc;
}

__attribute__((unused)) static void usb_pipe_selftest(void) {
    /* No-op self test placeholder. */
}
