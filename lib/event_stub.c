#include <event.h>
#include <stdint.h>

bool gui_event_poll(gui_event_t *out) {
    (void)out;
    return false;
}

void gui_event_push_keychar(char c) {
    (void)c;
}

void gui_event_push_mouse_move(int x, int y, int dx, int dy, uint8_t buttons) {
    (void)x; (void)y; (void)dx; (void)dy; (void)buttons;
}

void gui_event_push_mouse_scroll(int x, int y, int delta) {
    (void)x; (void)y; (void)delta;
}

