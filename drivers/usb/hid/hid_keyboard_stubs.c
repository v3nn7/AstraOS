#include <stdarg.h>
#include <stdbool.h>
#include <string.h>
#include <usb/usb_hid.h>
#include <usb/usb_device.h>
#include <event.h>
#include <klog.h>
#include <string.h>

int usb_hid_keyboard_init(usb_device_t *dev) {
    (void)dev;
    return 0;
}

int usb_hid_keyboard_read(usb_device_t *dev, uint8_t *modifiers, uint8_t keys[6]) {
    (void)dev;
    if (modifiers) {
        *modifiers = 0;
    }
    if (keys) {
        k_memset(keys, 0, 6);
    }
    return 0;
}

void usb_hid_process_keyboard_report(usb_device_t *dev, uint8_t *report, size_t len) {
    (void)dev;
    (void)report;
    (void)len;
}

bool gui_event_poll(gui_event_t *out) {
    (void)out;
    return false;
}

void gui_event_push_keychar(char c) {
    (void)c;
}

