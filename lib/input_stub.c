#include <input/input_core.h>
#include <kmalloc.h>
#include <string.h>

static input_device_t dummy_dev = { INPUT_DEVICE_UNKNOWN, "stub", false, NULL, NULL };

int input_core_init(void) { return 0; }
void input_core_cleanup(void) {}

input_device_t *input_device_register(input_device_type_t type, const char *name) {
    (void)type;
    (void)name;
    return &dummy_dev;
}

void input_device_unregister(input_device_t *dev) { (void)dev; }

void input_event_submit(input_device_t *dev, const input_event_t *event) {
    (void)dev;
    (void)event;
}

bool input_event_poll(input_event_t *event) {
    (void)event;
    return false;
}

void input_event_flush(void) {}

void input_key_press(input_device_t *dev, uint32_t keycode, uint8_t modifiers) {
    (void)dev; (void)keycode; (void)modifiers;
}
void input_key_release(input_device_t *dev, uint32_t keycode) {
    (void)dev; (void)keycode;
}
void input_key_char(input_device_t *dev, char ch) {
    (void)dev; (void)ch;
}
void input_mouse_move(input_device_t *dev, int32_t x, int32_t y, int32_t dx, int32_t dy, uint8_t buttons) {
    (void)dev; (void)x; (void)y; (void)dx; (void)dy; (void)buttons;
}
void input_mouse_button(input_device_t *dev, uint8_t button, bool pressed) {
    (void)dev; (void)button; (void)pressed;
}
void input_mouse_scroll(input_device_t *dev, int32_t delta) {
    (void)dev; (void)delta;
}

