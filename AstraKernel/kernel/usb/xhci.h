#pragma once
#include "types.h"
#include "usb_core.h"

void xhci_init(usb_controller_t *ctrl);
void xhci_irq_handler(void);
