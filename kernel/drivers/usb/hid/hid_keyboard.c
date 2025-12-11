/**
 * USB HID Keyboard Integration
 * 
 * Handles USB HID keyboard input and converts to AstraOS events.
 */

#include <drivers/usb/usb_hid.h>
#include <drivers/usb/usb_core.h>
#include <drivers/usb/usb_device.h>
#include "event.h"
#include "kernel.h"
#include "klog.h"

/* USB HID Keyboard State */
usb_device_t *usb_hid_keyboard_device = NULL;

/* Boot protocol keyboard scancode to ASCII mapping */
__attribute__((unused)) static const char usb_keyboard_scancode_map[128] = {
    0,   0,   '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b', '\t',
    'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n', 0,   'a', 's',
    'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`', 0,   '\\', 'z', 'x', 'c', 'v',
    'b', 'n', 'm', ',', '.', '/', 0,   '*', 0,   ' ', 0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   '7', '8', '9', '-', '4', '5', '6', '+', '1',
    '2', '3', '0', '.', 0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0
};

/**
 * Register USB HID keyboard device
 */
void usb_hid_register_keyboard(usb_device_t *dev) {
    if (!dev) return;
    
    if (usb_hid_keyboard_init(dev) == 0) {
        usb_hid_keyboard_device = dev;
        klog_printf(KLOG_INFO, "usb_hid: registered keyboard device");
    }
}

/**
 * Poll USB HID keyboard and update events
 */
void usb_hid_poll_keyboard(void) {
    if (!usb_hid_keyboard_device) {
        return;
    }

    /* Process keyboard report and generate input events */
    extern void usb_hid_process_keyboard_report(usb_device_t *dev, uint8_t *report, size_t len);
    
    /* Read report - this will populate the report buffer */
    uint8_t modifiers, keys[6];
    int ret = usb_hid_keyboard_read(usb_hid_keyboard_device, &modifiers, keys);
    if (ret < 0) {
        return; /* No data or error */
    }
    
    /* Get report buffer from device driver data */
    typedef struct {
        void *report_buffer;
        size_t report_size;
    } usb_hid_device_t;
    usb_hid_device_t *hid = (usb_hid_device_t *)usb_hid_keyboard_device->driver_data;
    if (hid && hid->report_buffer) {
        /* Process the report that was just read */
        usb_hid_process_keyboard_report(usb_hid_keyboard_device, 
                                        (uint8_t*)hid->report_buffer, 
                                        ret);
    }
}

/**
 * Check if USB HID keyboard is available
 */
bool usb_hid_keyboard_available(void) {
    return usb_hid_keyboard_device != NULL;
}

/**
 * Keyboard init wrapper (for drivers.h compatibility)
 */
void keyboard_init(void) {
    /* USB HID keyboard is initialized via usb_hid_init() */
    /* This function is called from kmain() */
}

/**
 * Read character from keyboard buffer
 */
bool keyboard_read_char(char *ch_out) {
    if (!ch_out) return false;
    
    /* Poll keyboard first (only if available) */
    if (usb_hid_keyboard_available()) {
        usb_hid_poll_keyboard();
    }
    
    /* Try to get character from event queue */
    extern bool gui_event_poll(gui_event_t *out);
    extern void gui_event_push_keychar(char c);
    gui_event_t ev;
    if (gui_event_poll(&ev)) {
        if (ev.type == GUI_EVENT_KEY_CHAR) {
            *ch_out = ev.key.ch;
            return true;
        }
    }
    
    return false;
}

/**
 * Poll character from keyboard (non-blocking)
 */
bool keyboard_poll_char(char *ch_out) {
    return keyboard_read_char(ch_out);
}

