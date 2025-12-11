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

#endif /* EHCI_H */
