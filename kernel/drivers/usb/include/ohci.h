#ifndef OHCI_H
#define OHCI_H

#include <stdint.h>
#include "usb_controller.h"

/* OHCI controller wrapper for legacy USB1 devices. */
typedef struct ohci_controller {
    usb_controller_t base;
    volatile uint32_t *regs;
} ohci_controller_t;

bool ohci_init(ohci_controller_t* ctrl, uintptr_t mmio_base);
bool ohci_reset_port(ohci_controller_t* ctrl, uint8_t port);
bool ohci_poll(ohci_controller_t* ctrl);

#endif /* OHCI_H */
