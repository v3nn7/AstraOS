/**
 * Minimal HID mouse handling for host tests.
 */

#include <drivers/usb/usb_hid.h>
#include "string.h"

void hid_mouse_reset(hid_mouse_state_t *state) {
    if (!state) {
        return;
    }
    state->delta_x = 0;
    state->delta_y = 0;
    state->wheel = 0;
    state->buttons = 0;
}

void hid_mouse_handle_report(hid_mouse_state_t *state, const uint8_t *report, size_t len) {
    if (!state || !report || len < 3) {
        return;
    }
    state->buttons = report[0];
    state->delta_x = (int8_t)report[1];
    state->delta_y = (int8_t)report[2];
    state->wheel = (len >= 4) ? (int8_t)report[3] : 0;
}
