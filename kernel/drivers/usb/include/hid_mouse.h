#ifndef HID_MOUSE_H
#define HID_MOUSE_H

#include <stdint.h>
#include <stddef.h>
#include "hid.h"

/* Mouse-specific HID state used by higher-level input handlers. */
typedef struct hid_mouse_state {
    int8_t delta_x;
    int8_t delta_y;
    int8_t wheel;
    uint8_t buttons;
} hid_mouse_state_t;

void hid_mouse_reset(hid_mouse_state_t* st);
void hid_mouse_handle_report(hid_mouse_state_t* st, const uint8_t* report, size_t len);

#endif /* HID_MOUSE_H */
