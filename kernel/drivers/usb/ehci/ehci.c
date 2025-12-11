#include "../include/ehci.h"
#include <stdbool.h>
#include <stdint.h>

typedef struct ehci_port_state {
    bool powered;
    bool reset;
} ehci_port_state_t;

static ehci_port_state_t g_ehci_ports[8];

bool ehci_port_power_state_set(uint8_t port, bool on) {
    if (port >= 8) return false;
    g_ehci_ports[port].powered = on;
    return true;
}

bool ehci_init(ehci_controller_t* ctrl, uintptr_t mmio_base) {
    if (!ctrl) return false;
    ctrl->base.type = USB_CONTROLLER_EHCI;
    ctrl->base.driver_data = ctrl;
    ctrl->cap_regs = (volatile uint32_t*)mmio_base;
    ctrl->op_regs = (volatile uint32_t*)(mmio_base ? (mmio_base + 0x20) : 0);
    for (int i = 0; i < 8; ++i) {
        g_ehci_ports[i].powered = false;
        g_ehci_ports[i].reset = false;
    }
    return true;
}

bool ehci_reset_port(ehci_controller_t* ctrl, uint8_t port) {
    (void)ctrl;
    if (port >= 8) return false;
    g_ehci_ports[port].reset = true;
    return true;
}

bool ehci_poll(ehci_controller_t* ctrl) {
    (void)ctrl;
    for (int i = 0; i < 8; ++i) {
        if (g_ehci_ports[i].powered) {
            return true;
        }
    }
    return true;
}
