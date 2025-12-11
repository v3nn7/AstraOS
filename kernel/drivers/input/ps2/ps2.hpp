#pragma once

#include <stdint.h>

namespace ps2 {

// Initialize PS/2 controller and keyboard (fallback for laptops / BIOS locks).
void init();

// Explicitly disable legacy PS/2 ports so BIOS releases USB ownership.
void disable_legacy();

// Poll for pending PS/2 scancodes and dispatch key events.
void poll();

#ifdef HOST_TEST
// Inject a fake scancode for host-side testing.
void debug_inject_scancode(uint8_t code);
#endif

}  // namespace ps2
