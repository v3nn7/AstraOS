#ifndef HID_MOUSE_H
#define HID_MOUSE_H

#include <stdint.h>
#include "hid.h"

/* Mouse-specific HID state used by higher-level input handlers. */
typedef struct hid_mouse_state {
    int8_t delta_x;
    int8_t delta_y;
    int8_t wheel;
    uint8_t buttons;
} hid_mouse_state_t;

#endif /* HID_MOUSE_H */
