#include "xhci.h"
#include "kernel.h"

int xhci_init_one(uint8_t bus, uint8_t slot, uint8_t func, uint16_t vendor, uint16_t device) {
    printf("XHCI: init stub at %02x:%02x.%x ven=%04x dev=%04x (not implemented yet)\n",
           bus, slot, func, vendor, device);
    return 0;
}

