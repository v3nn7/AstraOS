#pragma once

#include "types.h"

typedef enum {
    GUI_EVENT_NONE = 0,
    GUI_EVENT_KEY_CHAR,
    GUI_EVENT_MOUSE_MOVE,
    GUI_EVENT_MOUSE_BUTTON,
    GUI_EVENT_MOUSE_SCROLL
} gui_event_type_t;

typedef struct {
    gui_event_type_t type;
    union {
        struct { char ch; } key;
        struct { int x, y; int dx, dy; uint8_t buttons; } mouse;
    };
} gui_event_t;

/* Poll next event; returns false if queue empty */
bool gui_event_poll(gui_event_t *out);

/* Producers (drivers) */
void gui_event_push_keychar(char c);
void gui_event_push_mouse_move(int x, int y, int dx, int dy, uint8_t buttons);
void gui_event_push_mouse_button(int x, int y, uint8_t buttons);
void gui_event_push_mouse_scroll(int x, int y, int scroll);

