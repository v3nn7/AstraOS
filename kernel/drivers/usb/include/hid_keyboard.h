#ifndef HID_KEYBOARD_H
#define HID_KEYBOARD_H

#include <stdint.h>
#include "hid.h"

/* Keyboard-specific HID mapping metadata. */
typedef struct hid_keyboard_state {
    uint8_t modifier_mask;
    uint8_t key_codes[6];
} hid_keyboard_state_t;

#endif /* HID_KEYBOARD_H */
