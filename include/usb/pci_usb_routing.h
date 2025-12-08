#pragma once

#include <stdint.h>

typedef struct {
    uint8_t bus;
    uint8_t slot;
    uint8_t func;
    uint8_t prog_if;
} pci_device_t;

static inline void usb_route_ports(void) {}

void xhci_force_port_routing(pci_device_t *dev);

