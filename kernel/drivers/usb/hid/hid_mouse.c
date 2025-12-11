#include "../include/hid_mouse.h"
#include "../include/hid.h"

void hid_mouse_reset(hid_mouse_state_t* st) {
    if (!st) return;
    st->delta_x = 0;
    st->delta_y = 0;
    st->wheel = 0;
    st->buttons = 0;
}

void hid_mouse_handle_report(hid_mouse_state_t* st, const uint8_t* report, size_t len) {
    if (!st || !report || len < 3) {
        return;
    }
    st->buttons = report[0];
    st->delta_x = (int8_t)report[1];
    st->delta_y = (int8_t)report[2];
    st->wheel = (len >= 4) ? (int8_t)report[3] : 0;
}
