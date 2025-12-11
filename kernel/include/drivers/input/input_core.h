#pragma once

#include "types.h"

/**
 * Input Subsystem Core
 * 
 * Provides unified input event layer independent of hardware (USB HID, PS/2, etc.)
 */

/* Input Event Types */
typedef enum {
    INPUT_EVENT_NONE = 0,
    INPUT_EVENT_KEY_PRESS,
    INPUT_EVENT_KEY_RELEASE,
    INPUT_EVENT_KEY_CHAR,        /* ASCII character */
    INPUT_EVENT_MOUSE_MOVE,
    INPUT_EVENT_MOUSE_BUTTON,
    INPUT_EVENT_MOUSE_SCROLL,
    INPUT_EVENT_GAMEPAD_BUTTON,
    INPUT_EVENT_GAMEPAD_AXIS
} input_event_type_t;

/* Mouse Button IDs */
#define INPUT_MOUSE_BUTTON_LEFT    0x01
#define INPUT_MOUSE_BUTTON_RIGHT   0x02
#define INPUT_MOUSE_BUTTON_MIDDLE  0x04
#define INPUT_MOUSE_BUTTON_4       0x08
#define INPUT_MOUSE_BUTTON_5       0x10

/* Input Event Structure */
typedef struct {
    input_event_type_t type;
    uint64_t timestamp;           /* Timestamp in microseconds */
    
    union {
        struct {
            uint32_t keycode;     /* Scancode or HID usage */
            uint8_t modifiers;    /* Shift, Ctrl, Alt, etc. */
            char ascii;           /* ASCII character if available */
        } key;
        
        struct {
            int32_t x;
            int32_t y;
            int32_t dx;
            int32_t dy;
            uint8_t buttons;
        } mouse;
        
        struct {
            int32_t delta;        /* Scroll delta */
        } scroll;
        
        struct {
            uint32_t button_id;
            bool pressed;
        } gamepad;
    };
} input_event_t;

/* Input Device Types */
typedef enum {
    INPUT_DEVICE_KEYBOARD = 0,
    INPUT_DEVICE_MOUSE,
    INPUT_DEVICE_GAMEPAD,
    INPUT_DEVICE_TOUCHPAD,
    INPUT_DEVICE_TOUCHSCREEN
} input_device_type_t;

/* Input Device */
typedef struct input_device {
    input_device_type_t type;
    const char *name;
    bool enabled;
    void *driver_data;
    struct input_device *next;
} input_device_t;

/* Input Core Functions */
int input_core_init(void);
void input_core_cleanup(void);

/* Device Management */
input_device_t *input_device_register(input_device_type_t type, const char *name);
void input_device_unregister(input_device_t *dev);

/* Event Submission */
void input_event_submit(input_device_t *dev, const input_event_t *event);

/* Event Polling */
bool input_event_poll(input_event_t *event);
void input_event_flush(void);

/* Keyboard Helpers */
void input_key_press(input_device_t *dev, uint32_t keycode, uint8_t modifiers);
void input_key_release(input_device_t *dev, uint32_t keycode);
void input_key_char(input_device_t *dev, char ch);

/* Mouse Helpers */
void input_mouse_move(input_device_t *dev, int32_t x, int32_t y, int32_t dx, int32_t dy, uint8_t buttons);
void input_mouse_button(input_device_t *dev, uint8_t button, bool pressed);
void input_mouse_scroll(input_device_t *dev, int32_t delta);

