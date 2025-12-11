#pragma once

#include <drivers/usb/usb_core.h>
#include "types.h"

/**
 * USB Hub Support
 * 
 * Basic hub driver for port management and device discovery.
 */

/* Hub port status */
typedef struct {
    uint16_t port_status;
    uint16_t port_change;
} usb_hub_port_status_t;

/* Hub Functions */
int usb_hub_probe(usb_device_t *dev);
int usb_hub_init(usb_device_t *dev);
int usb_hub_get_port_status(usb_device_t *hub_dev, uint8_t port, usb_hub_port_status_t *status);
int usb_hub_reset_port(usb_device_t *hub_dev, uint8_t port);
int usb_hub_port_power_on(usb_device_t *hub_dev, uint8_t port);
int usb_hub_scan_ports(usb_device_t *hub_dev);

