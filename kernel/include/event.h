#pragma once

#include "types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    GUI_EVENT_NONE = 0,
    GUI_EVENT_KEY_CHAR,
    GUI_EVENT_MOUSE_MOVE,
    GUI_EVENT_MOUSE_SCROLL
} gui_event_type_t;

typedef struct {
    gui_event_type_t type;
    union {
        struct {
            char ch;
        } key;
        struct {
            int32_t x;
            int32_t y;
            int32_t dx;
            int32_t dy;
            uint8_t buttons;
        } mouse;
        struct {
            int32_t delta;
        } scroll;
    };
} gui_event_t;

bool gui_event_poll(gui_event_t *out);
void gui_event_push_keychar(char c);
void gui_event_push_mouse_move(int32_t x, int32_t y, int32_t dx, int32_t dy, uint8_t buttons);
void gui_event_push_mouse_scroll(int32_t x, int32_t y, int32_t delta);

#ifdef __cplusplus
}
#endif
