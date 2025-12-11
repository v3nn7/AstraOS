#pragma once

#include <stdint.h>

extern "C" {
#include "drivers/usb/include/usb_core.h"
}

namespace usb {

/* Initialize USB stack and log discovered stub controllers. */
void usb_init();

/* Poll USB stack; currently a lightweight placeholder. */
void usb_poll();

/* Accessors for stubbed controller/device counts. */
uint32_t controller_count();
uint32_t device_count();
const usb_controller_t* controller_at(uint32_t index);

}  // namespace usb
