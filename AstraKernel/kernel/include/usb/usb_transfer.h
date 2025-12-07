#pragma once

#include "usb/usb_core.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * USB Transfer Management
 * 
 * Provides unified transfer API for control, interrupt, bulk, and isochronous transfers.
 */

/* Transfer allocation */
usb_transfer_t *usb_transfer_alloc(usb_device_t *dev, usb_endpoint_t *ep, size_t length);
void usb_transfer_free(usb_transfer_t *transfer);

/* Transfer submission */
int usb_transfer_submit(usb_transfer_t *transfer);
int usb_transfer_cancel(usb_transfer_t *transfer);

/* Control transfer helpers */
int usb_control_transfer(usb_device_t *dev, uint8_t bmRequestType, uint8_t bRequest,
                         uint16_t wValue, uint16_t wIndex, void *data, uint16_t wLength,
                         uint32_t timeout_ms);

/* Interrupt transfer */
int usb_interrupt_transfer(usb_device_t *dev, usb_endpoint_t *ep, void *data,
                           size_t length, uint32_t timeout_ms);

/* Bulk transfer */
int usb_bulk_transfer(usb_device_t *dev, usb_endpoint_t *ep, void *data,
                      size_t length, uint32_t timeout_ms);

/* Setup packet builder */
void usb_build_setup_packet(uint8_t *setup, uint8_t bmRequestType, uint8_t bRequest,
                            uint16_t wValue, uint16_t wIndex, uint16_t wLength);

#ifdef __cplusplus
}
#endif

