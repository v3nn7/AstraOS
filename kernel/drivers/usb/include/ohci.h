#ifndef OHCI_H
#define OHCI_H

#include <stdint.h>
#include "usb_controller.h"

/* OHCI controller wrapper for legacy USB1 devices. */
typedef struct ohci_controller {
    usb_controller_t base;
    volatile uint32_t *regs;
} ohci_controller_t;

#endif /* OHCI_H */
