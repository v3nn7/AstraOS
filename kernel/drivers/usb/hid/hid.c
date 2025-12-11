/**
 * HID core stubs for host tests.
 */

#include <drivers/usb/usb_hid.h>
#include "string.h"

int usb_hid_init(void) {
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
    if (modifiers) *modifiers = 0;
    if (keys) {
        memset(keys, 0, 6);
    }
    return 0;
}

void usb_hid_process_keyboard_report(usb_device_t *dev, uint8_t *report, size_t len) {
    (void)dev;
    (void)report;
    (void)len;
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
