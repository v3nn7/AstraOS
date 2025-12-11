#pragma once

#include <drivers/usb/usb_core.h>
#include "types.h"

/**
 * USB Helper Functions Header
 * 
 * Declares common utility functions used across USB stack.
 */

/* CRC calculations */
uint8_t usb_crc5(uint16_t data, uint8_t nbits);
uint16_t usb_crc16(const uint8_t *data, size_t length);

/* String conversions */
const char *usb_speed_string(usb_speed_t speed);
const char *usb_controller_type_string(usb_controller_type_t type);
const char *usb_endpoint_type_string(uint8_t type);

/* Endpoint helpers */
uint8_t usb_endpoint_address(uint8_t bEndpointAddress);
uint8_t usb_endpoint_direction(uint8_t bEndpointAddress);
uint8_t usb_endpoint_type(uint8_t bmAttributes);
uint8_t usb_endpoint_sync_type(uint8_t bmAttributes);
uint8_t usb_endpoint_usage_type(uint8_t bmAttributes);

/* Frame/microframe calculations */
uint16_t usb_frame_from_microframe(uint32_t microframe);
uint8_t usb_microframe_from_frame(uint16_t frame);

/* Utility functions */
int usb_wait_for_condition(bool (*condition)(void), uint32_t timeout_us);
void usb_dump_descriptor(const uint8_t *data, size_t length);

