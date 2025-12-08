#ifndef ASTRAOS_EVENT_H
#define ASTRAOS_EVENT_H

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    GUI_EVENT_NONE = 0,
    GUI_EVENT_KEY_CHAR = 1
} gui_event_type_t;

typedef struct {
    gui_event_type_t type;
    struct {
        char ch;
    } key;
} gui_event_t;

bool gui_event_poll(gui_event_t *out);
void gui_event_push_keychar(char c);
void gui_event_push_mouse_move(int x, int y, int dx, int dy, uint8_t buttons);
void gui_event_push_mouse_scroll(int x, int y, int delta);

#endif /* ASTRAOS_EVENT_H */

