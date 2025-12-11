#ifndef HID_KEYBOARD_H
#define HID_KEYBOARD_H

#include <stdint.h>
#include <stddef.h>
#include "hid.h"

/* Keyboard-specific HID mapping metadata. */
typedef struct hid_keyboard_state {
    uint8_t modifier_mask;
    uint8_t key_codes[6];
} hid_keyboard_state_t;

void hid_keyboard_reset(hid_keyboard_state_t* state);
uint8_t hid_keyboard_usage_to_key(uint8_t usage);
void hid_keyboard_handle_report(hid_keyboard_state_t* state, const uint8_t* report, size_t len);

#endif /* HID_KEYBOARD_H */
