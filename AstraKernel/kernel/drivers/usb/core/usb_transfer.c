/**
 * USB Transfer Implementation
 * 
 * Handles all USB transfer types: control, interrupt, bulk, isochronous.
 */

#include "usb/usb_transfer.h"
#include "usb/usb_device.h"
#include "kmalloc.h"
#include "kernel.h"
#include "klog.h"
#include "string.h"

/**
 * Allocate a USB transfer structure
 */
usb_transfer_t *usb_transfer_alloc(usb_device_t *dev, usb_endpoint_t *ep, size_t length) {
    if (!dev || !dev->controller) {
        klog_printf(KLOG_ERROR, "usb_transfer: invalid device");
        return NULL;
    }

    usb_transfer_t *transfer = (usb_transfer_t *)kmalloc(sizeof(usb_transfer_t));
    if (!transfer) {
        klog_printf(KLOG_ERROR, "usb_transfer: allocation failed");
        return NULL;
    }

    k_memset(transfer, 0, sizeof(usb_transfer_t));
    transfer->device = dev;
    transfer->endpoint = ep;
    transfer->length = length;
    transfer->timeout_ms = 5000; /* Default 5 second timeout */
    transfer->status = USB_TRANSFER_SUCCESS;

    if (length > 0) {
        transfer->buffer = (uint8_t *)kmalloc(length);
        if (!transfer->buffer) {
            kfree(transfer);
            klog_printf(KLOG_ERROR, "usb_transfer: buffer allocation failed");
            return NULL;
        }
    }

    return transfer;
}

/**
 * Free a USB transfer structure
 */
void usb_transfer_free(usb_transfer_t *transfer) {
    if (!transfer) return;

    if (transfer->buffer) {
        kfree(transfer->buffer);
    }

    if (transfer->controller_private) {
        kfree(transfer->controller_private);
    }

    kfree(transfer);
}

/**
 * Build USB setup packet
 */
void usb_build_setup_packet(uint8_t *setup, uint8_t bmRequestType, uint8_t bRequest,
                            uint16_t wValue, uint16_t wIndex, uint16_t wLength) {
    if (!setup) {
        klog_printf(KLOG_ERROR, "usb_transfer: setup packet buffer is NULL");
        return;
    }
    
    setup[0] = bmRequestType;
    setup[1] = bRequest;
    setup[2] = wValue & 0xFF;
    setup[3] = (wValue >> 8) & 0xFF;
    setup[4] = wIndex & 0xFF;
    setup[5] = (wIndex >> 8) & 0xFF;
    setup[6] = wLength & 0xFF;
    setup[7] = (wLength >> 8) & 0xFF;
}

/**
 * Submit a USB transfer
 */
int usb_transfer_submit(usb_transfer_t *transfer) {
    if (!transfer || !transfer->device || !transfer->device->controller) {
        klog_printf(KLOG_ERROR, "usb_transfer: invalid transfer");
        return -1;
    }

    usb_host_controller_t *hc = transfer->device->controller;
    if (!hc->ops) {
        klog_printf(KLOG_ERROR, "usb_transfer: controller has no ops");
        return -1;
    }

    /* Determine transfer type and call appropriate handler */
    if (transfer->is_control) {
        if (!hc->ops->transfer_control) {
            klog_printf(KLOG_ERROR, "usb_transfer: controller doesn't support control transfers");
            return -1;
        }
        return hc->ops->transfer_control(hc, transfer);
    }

    if (!transfer->endpoint) {
        klog_printf(KLOG_ERROR, "usb_transfer: no endpoint specified");
        return -1;
    }

    uint8_t type = transfer->endpoint->type;
    switch (type) {
        case USB_ENDPOINT_XFER_INT:
            if (!hc->ops->transfer_interrupt) {
                klog_printf(KLOG_ERROR, "usb_transfer: controller doesn't support interrupt transfers");
                return -1;
            }
            return hc->ops->transfer_interrupt(hc, transfer);

        case USB_ENDPOINT_XFER_BULK:
            if (!hc->ops->transfer_bulk) {
                klog_printf(KLOG_ERROR, "usb_transfer: controller doesn't support bulk transfers");
                return -1;
            }
            return hc->ops->transfer_bulk(hc, transfer);

        case USB_ENDPOINT_XFER_ISOC:
            if (!hc->ops->transfer_isoc) {
                klog_printf(KLOG_ERROR, "usb_transfer: controller doesn't support isochronous transfers");
                return -1;
            }
            return hc->ops->transfer_isoc(hc, transfer);

        default:
            klog_printf(KLOG_ERROR, "usb_transfer: unknown transfer type %d", type);
            return -1;
    }
}

/**
 * Cancel a USB transfer
 */
int usb_transfer_cancel(usb_transfer_t *transfer) {
    if (!transfer) return -1;

    transfer->status = USB_TRANSFER_ERROR;
    /* Controller-specific cancellation would be handled here */
    return 0;
}

/**
 * Control transfer helper
 */
int usb_control_transfer(usb_device_t *dev, uint8_t bmRequestType, uint8_t bRequest,
                         uint16_t wValue, uint16_t wIndex, void *data, uint16_t wLength,
                         uint32_t timeout_ms) {
    if (!dev || !dev->controller) {
        return -1;
    }

    /* Find control endpoint (endpoint 0) */
    usb_endpoint_t *ep = usb_device_find_endpoint(dev, 0x00);
    if (!ep) {
        /* Create default control endpoint */
        ep = usb_endpoint_alloc(dev, 0x00, USB_ENDPOINT_XFER_CONTROL, 64, 0);
        if (!ep) {
            klog_printf(KLOG_ERROR, "usb_transfer: failed to create control endpoint");
            return -1;
        }
    }

    /* Allocate transfer */
    usb_transfer_t *transfer = usb_transfer_alloc(dev, ep, wLength);
    if (!transfer) {
        return -1;
    }

    transfer->is_control = true;
    transfer->timeout_ms = timeout_ms;

    /* Build setup packet */
    usb_build_setup_packet(transfer->setup, bmRequestType, bRequest, wValue, wIndex, wLength);

    /* Copy data for OUT transfers */
    if ((bmRequestType & USB_ENDPOINT_DIR_IN) == 0 && data && wLength > 0) {
        memcpy(transfer->buffer, data, wLength);
    }

    /* Submit transfer */
    int ret = usb_transfer_submit(transfer);
    if (ret != 0) {
        usb_transfer_free(transfer);
        return ret;
    }

    /* Copy data for IN transfers */
    if ((bmRequestType & USB_ENDPOINT_DIR_IN) != 0 && data && transfer->actual_length > 0) {
        size_t copy_len = (transfer->actual_length < wLength) ? transfer->actual_length : wLength;
        memcpy(data, transfer->buffer, copy_len);
    }

    ret = (transfer->status == USB_TRANSFER_SUCCESS) ? (int)transfer->actual_length : -1;
    usb_transfer_free(transfer);
    return ret;
}

/**
 * Interrupt transfer helper
 */
int usb_interrupt_transfer(usb_device_t *dev, usb_endpoint_t *ep, void *data,
                           size_t length, uint32_t timeout_ms) {
    if (!dev || !ep) {
        return -1;
    }

    usb_transfer_t *transfer = usb_transfer_alloc(dev, ep, length);
    if (!transfer) {
        return -1;
    }

    transfer->timeout_ms = timeout_ms;
    if (data && length > 0) {
        memcpy(transfer->buffer, data, length);
    }

    int ret = usb_transfer_submit(transfer);
    if (ret == 0 && transfer->status == USB_TRANSFER_SUCCESS && data) {
        size_t copy_len = (transfer->actual_length < length) ? transfer->actual_length : length;
        memcpy(data, transfer->buffer, copy_len);
    }

    ret = (transfer->status == USB_TRANSFER_SUCCESS) ? (int)transfer->actual_length : -1;
    usb_transfer_free(transfer);
    return ret;
}

/**
 * Bulk transfer helper
 */
int usb_bulk_transfer(usb_device_t *dev, usb_endpoint_t *ep, void *data,
                      size_t length, uint32_t timeout_ms) {
    if (!dev || !ep) {
        klog_printf(KLOG_ERROR, "usb_transfer: invalid parameters for bulk transfer");
        return -1;
    }

    if (length == 0) {
        klog_printf(KLOG_ERROR, "usb_transfer: bulk transfer length is zero");
        return -1;
    }

    usb_transfer_t *transfer = usb_transfer_alloc(dev, ep, length);
    if (!transfer) {
        return -1;
    }

    transfer->timeout_ms = timeout_ms;
    if (data && length > 0) {
        memcpy(transfer->buffer, data, length);
    }

    int ret = usb_transfer_submit(transfer);
    if (ret == 0 && transfer->status == USB_TRANSFER_SUCCESS && data) {
        size_t copy_len = (transfer->actual_length < length) ? transfer->actual_length : length;
        memcpy(data, transfer->buffer, copy_len);
    }

    ret = (transfer->status == USB_TRANSFER_SUCCESS) ? (int)transfer->actual_length : -1;
    usb_transfer_free(transfer);
    return ret;
}

