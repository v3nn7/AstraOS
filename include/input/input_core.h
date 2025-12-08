#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

typedef enum {
    INPUT_DEVICE_KEYBOARD = 0,
    INPUT_DEVICE_MOUSE = 1,
    INPUT_DEVICE_UNKNOWN = 255
} input_device_type_t;

#define INPUT_MOUSE_BUTTON_LEFT   0x01
#define INPUT_MOUSE_BUTTON_RIGHT  0x02
#define INPUT_MOUSE_BUTTON_MIDDLE 0x04

typedef enum {
    INPUT_EVENT_KEY_PRESS = 0,
    INPUT_EVENT_KEY_RELEASE,
    INPUT_EVENT_KEY_CHAR,
    INPUT_EVENT_MOUSE_MOVE,
    INPUT_EVENT_MOUSE_BUTTON,
    INPUT_EVENT_MOUSE_SCROLL
} input_event_type_t;

typedef struct input_device {
    input_device_type_t type;
    const char *name;
    bool enabled;
    void *driver_data;
    struct input_device *next;
} input_device_t;

typedef struct {
    input_event_type_t type;
    union {
        struct { uint32_t keycode; uint8_t modifiers; char ascii; } key;
        struct { int32_t x, y, dx, dy; uint8_t buttons; } mouse;
        struct { int32_t delta; } scroll;
    };
} input_event_t;

#ifdef __cplusplus
extern "C" {
#endif

int input_core_init(void);
void input_core_cleanup(void);
input_device_t *input_device_register(input_device_type_t type, const char *name);
void input_device_unregister(input_device_t *dev);
void input_event_submit(input_device_t *dev, const input_event_t *event);
bool input_event_poll(input_event_t *event);
void input_event_flush(void);

void input_key_press(input_device_t *dev, uint32_t keycode, uint8_t modifiers);
void input_key_release(input_device_t *dev, uint32_t keycode);
void input_key_char(input_device_t *dev, char ch);
void input_mouse_move(input_device_t *dev, int32_t x, int32_t y, int32_t dx, int32_t dy, uint8_t buttons);
void input_mouse_button(input_device_t *dev, uint8_t button, bool pressed);
void input_mouse_scroll(input_device_t *dev, int32_t delta);

#ifdef __cplusplus
}
#endif

