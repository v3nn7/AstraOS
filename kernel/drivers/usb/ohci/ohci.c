#include "../include/ohci.h"
#include <stdbool.h>
#include <stdint.h>

typedef struct ohci_port_state {
    bool powered;
    bool reset;
} ohci_port_state_t;

static ohci_port_state_t g_ohci_ports[8];

bool ohci_port_power_state_set(uint8_t port, bool on) {
    if (port >= 8) return false;
    g_ohci_ports[port].powered = on;
    return true;
}

bool ohci_init(ohci_controller_t* ctrl, uintptr_t mmio_base) {
    if (!ctrl) return false;
    ctrl->base.type = USB_CONTROLLER_OHCI;
    ctrl->base.driver_data = ctrl;
    ctrl->regs = (volatile uint32_t*)mmio_base;
    for (int i = 0; i < 8; ++i) {
        g_ohci_ports[i].powered = false;
        g_ohci_ports[i].reset = false;
    }
    return true;
}

bool ohci_reset_port(ohci_controller_t* ctrl, uint8_t port) {
    (void)ctrl;
    if (port >= 8) return false;
    g_ohci_ports[port].reset = true;
    return true;
}

bool ohci_poll(ohci_controller_t* ctrl) {
    (void)ctrl;
    /* If any port is powered, claim success. */
    for (int i = 0; i < 8; ++i) {
        if (g_ohci_ports[i].powered) {
            return true;
        }
    }
    return true;
}
