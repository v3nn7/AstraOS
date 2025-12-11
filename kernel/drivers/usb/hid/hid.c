/**
 * HID core with simple boot-keyboard parser.
 */

#include <drivers/usb/usb_hid.h>
#include <drivers/input/input_core.h>
#include "string.h"

extern void input_push_key(uint8_t key, bool pressed);

typedef struct {
    uint8_t modifiers;
    uint8_t keys[6];
} keyboard_ctx_t;

static keyboard_ctx_t g_kbd_state;

int usb_hid_init(void) {
    memset(&g_kbd_state, 0, sizeof(g_kbd_state));
    return 0;
}

void hid_init(void) {
    (void)usb_hid_init();
}

int usb_hid_probe_device(usb_device_t *dev) {
    (void)dev;
    return 0;
}

int usb_hid_mouse_init(usb_device_t *dev) {
    (void)dev;
    return 0;
}

int usb_hid_keyboard_init(usb_device_t *dev) {
    (void)dev;
    return 0;
}

static char hid_usage_to_ascii(uint8_t usage, uint8_t modifiers) {
    const bool shift = (modifiers & 0x22) != 0; /* LShift or RShift */
    if (usage >= 0x04 && usage <= 0x1d) { /* a-z */
        char base = shift ? 'A' : 'a';
        return (char)(base + (usage - 0x04));
    }
    if (usage >= 0x1e && usage <= 0x27) { /* 1-0 */
        static const char shifted_nums[] = {'!', '@', '#', '$', '%', '^', '&', '*', '(', ')'};
        uint8_t idx = usage - 0x1e;
        return shift ? shifted_nums[idx] : (char)('1' + idx % 10);
    }
    switch (usage) {
        case 0x2c: return ' ';
        case 0x28: return '\n';
        case 0x2a: return '\b';
        case 0x2d: return shift ? '_' : '-';
        case 0x2e: return shift ? '+' : '=';
        case 0x2f: return shift ? '{' : '[';
        case 0x30: return shift ? '}' : ']';
        case 0x31: return shift ? '|' : '\\';
        case 0x33: return shift ? ':' : ';';
        case 0x34: return shift ? '"' : '\'';
        case 0x36: return shift ? '>' : '.';
        case 0x37: return shift ? '?' : '/';
        case 0x35: return shift ? '~' : '`';
        default: return 0;
    }
}

static bool contains_key(const uint8_t *keys, uint8_t key) {
    for (int i = 0; i < 6; i++) {
        if (keys[i] == key) return true;
    }
    return false;
}

int usb_hid_mouse_read(usb_device_t *dev, int8_t *dx, int8_t *dy, uint8_t *buttons, int8_t *wheel) {
    (void)dev;
    if (dx) *dx = 0;
    if (dy) *dy = 0;
    if (buttons) *buttons = 0;
    if (wheel) *wheel = 0;
    return 0;
}

int usb_hid_keyboard_read(usb_device_t *dev, uint8_t *modifiers, uint8_t *keys) {
    (void)dev;
    if (modifiers) *modifiers = g_kbd_state.modifiers;
    if (keys) memcpy(keys, g_kbd_state.keys, sizeof(g_kbd_state.keys));
    return 0;
}

void usb_hid_process_keyboard_report(usb_device_t *dev, uint8_t *report, size_t len) {
    (void)dev;
    if (!report || len < 8) return;

    uint8_t new_mod = report[0];
    uint8_t new_keys[6] = {0};
    memcpy(new_keys, &report[2], 6);

    /* Handle releases: keys that were pressed but are now gone */
    for (int i = 0; i < 6; i++) {
        uint8_t prev = g_kbd_state.keys[i];
        if (prev && !contains_key(new_keys, prev)) {
            char ch = hid_usage_to_ascii(prev, g_kbd_state.modifiers);
            if (ch) input_push_key((uint8_t)ch, false);
            else input_push_key(prev, false);
        }
    }

    /* Handle new presses */
    for (int i = 0; i < 6; i++) {
        uint8_t key = new_keys[i];
        if (key && !contains_key(g_kbd_state.keys, key)) {
            char ch = hid_usage_to_ascii(key, new_mod);
            if (ch) input_push_key((uint8_t)ch, true);
            else input_push_key(key, true);
        }
    }

    g_kbd_state.modifiers = new_mod;
    memcpy(g_kbd_state.keys, new_keys, sizeof(new_keys));
}

void usb_hid_process_mouse_report(usb_device_t *dev, uint8_t *report, size_t len) {
    (void)dev;
    (void)report;
    (void)len;
}

void usb_hid_scan_devices(void) {}
void usb_hid_register_mouse(usb_device_t *dev) { (void)dev; }
void usb_hid_register_keyboard(usb_device_t *dev) { (void)dev; }
void usb_hid_poll_mouse(void) {}
void usb_hid_poll_keyboard(void) {}
int usb_hid_mouse_get_x(void) { return 0; }
int usb_hid_mouse_get_y(void) { return 0; }
bool usb_hid_mouse_available(void) { return true; }
bool usb_hid_keyboard_available(void) { return true; }
