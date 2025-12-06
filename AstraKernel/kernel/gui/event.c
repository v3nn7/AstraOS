#include "event.h"

#define GUI_EVENT_QUEUE 128

static gui_event_t queue[GUI_EVENT_QUEUE];
static volatile uint8_t q_head = 0, q_tail = 0;

static inline bool queue_full(void) {
    uint8_t next = (uint8_t)((q_head + 1) % GUI_EVENT_QUEUE);
    return next == q_tail;
}

static inline bool queue_empty(void) {
    return q_head == q_tail;
}

static void push(gui_event_t ev) {
    if (queue_full()) return;
    queue[q_head] = ev;
    q_head = (uint8_t)((q_head + 1) % GUI_EVENT_QUEUE);
}

bool gui_event_poll(gui_event_t *out) {
    if (queue_empty()) return false;
    *out = queue[q_tail];
    q_tail = (uint8_t)((q_tail + 1) % GUI_EVENT_QUEUE);
    return true;
}

void gui_event_push_keychar(char c) {
    gui_event_t ev = { .type = GUI_EVENT_KEY_CHAR };
    ev.key.ch = c;
    push(ev);
}

void gui_event_push_mouse_move(int x, int y, int dx, int dy, uint8_t buttons) {
    gui_event_t ev = { .type = GUI_EVENT_MOUSE_MOVE };
    ev.mouse.x = x; ev.mouse.y = y; ev.mouse.dx = dx; ev.mouse.dy = dy; ev.mouse.buttons = buttons;
    push(ev);
}

void gui_event_push_mouse_button(int x, int y, uint8_t buttons) {
    gui_event_t ev = { .type = GUI_EVENT_MOUSE_BUTTON };
    ev.mouse.x = x; ev.mouse.y = y; ev.mouse.buttons = buttons; ev.mouse.dx = 0; ev.mouse.dy = 0;
    push(ev);
}

