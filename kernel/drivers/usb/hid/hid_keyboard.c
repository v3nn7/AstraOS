/**
 * Minimal HID keyboard handling for host tests.
 */

#include <drivers/usb/usb_hid.h>
#include "string.h"

void hid_keyboard_reset(hid_keyboard_state_t *state) {
    if (!state) {
        return;
    }
    memset(state->last_keys, 0, sizeof(state->last_keys));
}

static void push_keys(const uint8_t *keys, size_t count, bool pressed) {
    extern void input_push_key(uint8_t key, bool pressed);
    if (!keys) {
        return;
    }
    for (size_t i = 0; i < count; i++) {
        if (keys[i] != 0) {
            /* Very small mapping: HID usage 0x04 -> 'a' */
            input_push_key((uint8_t)('a' + (keys[i] - 0x04)), pressed);
        }
    }
}

void hid_keyboard_handle_report(hid_keyboard_state_t *state, const uint8_t *report, size_t len) {
    if (!report || len < 8) {
        return;
    }
    const uint8_t *keys = report + 2; /* skip modifiers + reserved */
    push_keys(keys, 6, true);
    if (state) {
        push_keys(state->last_keys, 6, false);
        memcpy(state->last_keys, keys, 6);
    }
}
