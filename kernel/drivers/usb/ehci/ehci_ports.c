#include "../include/ehci.h"
#include <stdbool.h>
#include <stdint.h>

bool ehci_port_power(ehci_controller_t* ctrl, uint8_t port, bool on) {
    (void)ctrl;
    if (port >= 8) {
        return false;
    }
    extern bool ehci_port_power_state_set(uint8_t port, bool on);
    return ehci_port_power_state_set(port, on);
}
