#include "../include/xhci.h"

#include <stddef.h>
#include <stdint.h>

uint8_t xhci_ports_scan(xhci_controller_t* ctrl) {
    (void)ctrl;
    /* Stub: would read PORTSC for each port, here return 0. */
    return 0;
}
#include "../include/xhci.h"

/* Port state handling and link training logic will be added here. */
