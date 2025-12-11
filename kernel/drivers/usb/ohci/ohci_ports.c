#include "../include/ohci.h"
#include <stdbool.h>
#include <stdint.h>

bool ohci_port_power(ohci_controller_t* ctrl, uint8_t port, bool on) {
    (void)ctrl;
    if (port >= 8) {
        return false;
    }
    /* Track power in shared state from ohci.c */
    extern bool ohci_port_power_state_set(uint8_t port, bool on);
    return ohci_port_power_state_set(port, on);
}
