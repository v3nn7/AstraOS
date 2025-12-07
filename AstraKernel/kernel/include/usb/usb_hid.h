#pragma once

#include "usb/usb_core.h"
#include "types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * USB HID (Human Interface Device) Support
 * 
 * Provides HID mouse and keyboard drivers with report parsing.
 */

/* HID Request Codes */
#define HID_REQ_GET_REPORT       0x01
#define HID_REQ_GET_IDLE         0x02
#define HID_REQ_GET_PROTOCOL     0x03
#define HID_REQ_SET_REPORT       0x09
#define HID_REQ_SET_IDLE         0x0A
#define HID_REQ_SET_PROTOCOL     0x0B

/* HID Protocol Types */
#define HID_PROTOCOL_BOOT        0
#define HID_PROTOCOL_REPORT      1

/* HID Report Types */
#define HID_REPORT_TYPE_INPUT    1
#define HID_REPORT_TYPE_OUTPUT   2
#define HID_REPORT_TYPE_FEATURE  3

/* HID Mouse Button Masks */
#define HID_MOUSE_BUTTON_LEFT    0x01
#define HID_MOUSE_BUTTON_RIGHT   0x02
#define HID_MOUSE_BUTTON_MIDDLE  0x04

/* HID Functions */
int usb_hid_init(void);
void hid_init(void); /* Wrapper for drivers.h compatibility */
int usb_hid_probe_device(usb_device_t *dev);
int usb_hid_mouse_init(usb_device_t *dev);
int usb_hid_keyboard_init(usb_device_t *dev);

/* HID Mouse Functions */
int usb_hid_mouse_read(usb_device_t *dev, int8_t *dx, int8_t *dy, uint8_t *buttons, int8_t *wheel);

/* HID Keyboard Functions */
int usb_hid_keyboard_read(usb_device_t *dev, uint8_t *modifiers, uint8_t *keys);
void usb_hid_process_keyboard_report(usb_device_t *dev, uint8_t *report, size_t len);
void usb_hid_process_mouse_report(usb_device_t *dev, uint8_t *report, size_t len);

/* Integration Functions */
void usb_hid_scan_devices(void);
void usb_hid_register_mouse(usb_device_t *dev);
void usb_hid_register_keyboard(usb_device_t *dev);
void usb_hid_poll_mouse(void);
void usb_hid_poll_keyboard(void);
int usb_hid_mouse_get_x(void);
int usb_hid_mouse_get_y(void);
bool usb_hid_mouse_available(void);
bool usb_hid_keyboard_available(void);

#ifdef __cplusplus
}
#endif

