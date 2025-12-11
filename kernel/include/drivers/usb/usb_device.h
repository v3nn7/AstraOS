#pragma once

#include <drivers/usb/usb_core.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * USB Device Management
 * 
 * Handles device allocation, enumeration, configuration, and lifecycle.
 */

/* Device allocation and lifecycle */
usb_device_t *usb_device_alloc(void);
void usb_device_free(usb_device_t *dev);

/* Device enumeration */
int usb_device_enumerate(usb_device_t *dev);
int usb_device_set_address(usb_device_t *dev, uint8_t address);
int usb_device_set_configuration(usb_device_t *dev, uint8_t config);
int usb_device_get_configuration(usb_device_t *dev, uint8_t *config);

/* Driver binding */
int usb_bind_driver(usb_device_t *dev);

/* Device state management */
int usb_device_set_state(usb_device_t *dev, usb_device_state_t state);
usb_device_state_t usb_device_get_state(usb_device_t *dev);

/* Endpoint management */
int usb_device_add_endpoint(usb_device_t *dev, uint8_t address,
                            uint8_t attributes, uint16_t max_packet_size,
                            uint8_t interval);
usb_endpoint_t *usb_device_find_endpoint(usb_device_t *dev, uint8_t address);

/* Internal functions (used by core) */
void usb_device_list_add(usb_device_t *dev);
void usb_device_list_remove(usb_device_t *dev);
uint8_t usb_allocate_device_address(void);

#ifdef __cplusplus
}
#endif

