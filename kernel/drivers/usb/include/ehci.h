#ifndef EHCI_H
#define EHCI_H

#include <stdint.h>
#include "usb_controller.h"

/* EHCI controller wrapper for USB2 high-speed devices. */
typedef struct ehci_controller {
    usb_controller_t base;
    volatile uint32_t *cap_regs;
    volatile uint32_t *op_regs;
} ehci_controller_t;

bool ehci_init(ehci_controller_t* ctrl, uintptr_t mmio_base);
bool ehci_reset_port(ehci_controller_t* ctrl, uint8_t port);
bool ehci_poll(ehci_controller_t* ctrl);

#endif /* EHCI_H */
