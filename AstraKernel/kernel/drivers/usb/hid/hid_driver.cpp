/**
 * USB HID Implementation
 * 
 * Handles HID mouse and keyboard devices with report parsing using HID parser.
 */

#include "usb/usb_hid.h"
#include "usb/usb_device.h"
#include "usb/usb_transfer.h"
#include "usb/usb_descriptors.h"
#include "usb/hid/hid_parser.hpp"
#include "event.h"
#include "kmalloc.h"
#include "kernel.h"
#include "klog.h"
#include "string.h"

/* Input core functions are in C, need extern "C" */
extern "C" {
#include "input/input_core.h"
}

/* HID Device Private Data */
typedef struct {
    usb_device_t *device;
    usb_endpoint_t *intr_in_ep;
    uint8_t protocol;
    bool is_mouse;
    bool is_keyboard;
    void *report_buffer;
    size_t report_size;
    HIDParser *parser; /* C++ HID parser */
    input_device_t *input_dev; /* Registered input device */
    uint8_t last_keys[6]; /* Last keyboard state for key release detection */
    uint8_t last_modifiers; /* Last modifier state */
} usb_hid_device_t;

/**
 * Initialize HID subsystem
 */
int usb_hid_init(void) {
    klog_printf(KLOG_INFO, "usb_hid: initialized");
    return 0;
}

/**
 * HID init wrapper (for drivers.h compatibility)
 */
void hid_init(void) {
    usb_hid_init();
}

/**
 * Probe device for HID support
 */
int usb_hid_probe_device(usb_device_t *dev) {
    if (!dev) return -1;

    /* Check if device is HID class */
    if (dev->device_class == 0x03) { /* HID Class */
        klog_printf(KLOG_INFO, "usb_hid: found HID device VID:PID=%04x:%04x",
                    dev->vendor_id, dev->product_id);
        return 0;
    }

    /* Check interfaces - look for HID interface in configuration */
    /* TODO: Parse interface descriptors to find HID interfaces */
    /* For now, assume device class 0x03 means HID */
    return -1;
}

/**
 * Initialize HID mouse
 */
int usb_hid_mouse_init(usb_device_t *dev) {
    if (!dev) return -1;

    usb_hid_device_t *hid = (usb_hid_device_t *)kmalloc(sizeof(usb_hid_device_t));
    if (!hid) {
        return -1;
    }

    k_memset(hid, 0, sizeof(usb_hid_device_t));
    hid->device = dev;
    hid->is_mouse = true;
    hid->protocol = HID_PROTOCOL_BOOT; /* Start with boot protocol */
    dev->driver_data = hid;

    /* Find interrupt IN endpoint */
    for (uint8_t i = 0; i < dev->num_endpoints; i++) {
        usb_endpoint_t *ep = &dev->endpoints[i];
        if ((ep->address & USB_ENDPOINT_DIR_IN) != 0 && ep->type == USB_ENDPOINT_XFER_INT) {
            hid->intr_in_ep = ep;
            klog_printf(KLOG_INFO, "usb_hid: mouse interrupt endpoint 0x%02x", ep->address);
            break;
        }
    }

    if (!hid->intr_in_ep) {
        klog_printf(KLOG_ERROR, "usb_hid: mouse has no interrupt IN endpoint");
        kfree(hid);
        return -1;
    }

    /* Initialize HID parser */
    hid->parser = new HIDParser(dev);
    if (!hid->parser) {
        klog_printf(KLOG_ERROR, "usb_hid: failed to allocate parser");
        kfree(hid);
        return -1;
    }

    if (hid->parser->init() < 0) {
        klog_printf(KLOG_ERROR, "usb_hid: failed to initialize parser");
        delete hid->parser;
        kfree(hid);
        return -1;
    }

    /* Get report size */
    hid->report_size = hid->parser->get_input_report_size(0);
    if (hid->report_size == 0) {
        hid->report_size = 4; /* Boot protocol mouse: 4 bytes */
    }
    hid->report_buffer = kmalloc(hid->report_size);
    if (!hid->report_buffer) {
        delete hid->parser;
        kfree(hid);
        return -1;
    }

    /* HID Boot Protocol Setup Sequence */
    /* Step 1: Set IDLE (disable automatic reports) */
    usb_control_transfer(dev, USB_REQ_TYPE_CLASS | USB_REQ_TYPE_INTERFACE | USB_ENDPOINT_DIR_OUT,
                        HID_REQ_SET_IDLE, 0, 0, NULL, 0, 1000);
    
    /* Step 2: Set Boot Protocol (0) for simple mouse format */
    int ret = usb_control_transfer(dev, USB_REQ_TYPE_CLASS | USB_REQ_TYPE_INTERFACE | USB_ENDPOINT_DIR_OUT,
                                   HID_REQ_SET_PROTOCOL, HID_PROTOCOL_BOOT, 0, NULL, 0, 1000);
    if (ret < 0) {
        klog_printf(KLOG_WARN, "usb_hid: failed to set boot protocol, trying report protocol");
        /* Fallback to report protocol */
        hid->protocol = HID_PROTOCOL_REPORT;
        usb_control_transfer(dev, USB_REQ_TYPE_CLASS | USB_REQ_TYPE_INTERFACE | USB_ENDPOINT_DIR_OUT,
                            HID_REQ_SET_PROTOCOL, HID_PROTOCOL_REPORT, 0, NULL, 0, 1000);
    } else {
        hid->protocol = HID_PROTOCOL_BOOT;
        klog_printf(KLOG_INFO, "usb_hid: mouse set to boot protocol");
    }

    /* Register input device */
    static char dev_name[64];
    /* Use static buffer for device name (input_core stores pointer) */
    k_memset(dev_name, 0, sizeof(dev_name));
    /* Format: "USB Mouse VID:PID" - simple hex formatting */
    char *p = dev_name;
    memcpy(p, "USB Mouse ", 10);
    p += 10;
    /* Format VID */
    uint16_t vid = dev->vendor_id;
    for (int i = 0; i < 4; i++) {
        uint8_t nibble = (vid >> (12 - i * 4)) & 0xF;
        *p++ = (nibble < 10) ? ('0' + nibble) : ('A' + nibble - 10);
    }
    *p++ = ':';
    /* Format PID */
    uint16_t pid = dev->product_id;
    for (int i = 0; i < 4; i++) {
        uint8_t nibble = (pid >> (12 - i * 4)) & 0xF;
        *p++ = (nibble < 10) ? ('0' + nibble) : ('A' + nibble - 10);
    }
    *p = '\0';
    hid->input_dev = input_device_register(INPUT_DEVICE_MOUSE, dev_name);
    if (!hid->input_dev) {
        klog_printf(KLOG_ERROR, "usb_hid: failed to register mouse input device");
        kfree(hid->report_buffer);
        delete hid->parser;
        kfree(hid);
        return -1;
    }
    hid->input_dev->driver_data = hid;

    klog_printf(KLOG_INFO, "usb_hid: mouse initialized (protocol=%u, report_size=%zu)", 
                hid->protocol, hid->report_size);
    return 0;
}

/**
 * Read mouse data using HID parser or boot protocol
 */
int usb_hid_mouse_read(usb_device_t *dev, int8_t *dx, int8_t *dy, uint8_t *buttons, int8_t *wheel) {
    if (!dev || !dx || !dy || !buttons) return -1;

    usb_hid_device_t *hid = (usb_hid_device_t *)dev->driver_data;
    if (!hid || !hid->is_mouse || !hid->intr_in_ep) return -1;

    /* Read report (short timeout for non-blocking) */
    int ret = usb_interrupt_transfer(dev, hid->intr_in_ep, hid->report_buffer, hid->report_size, 10);
    if (ret < 0) {
        return -1;
    }

    /* Boot Protocol: 4 bytes [buttons, dx, dy, wheel] */
    if (hid->protocol == HID_PROTOCOL_BOOT) {
        if (ret >= 3) {
            uint8_t *buf = (uint8_t*)hid->report_buffer;
            *buttons = buf[0];
            *dx = (int8_t)buf[1];
            *dy = (int8_t)buf[2];
            if (wheel && ret >= 4) {
                *wheel = (int8_t)buf[3];
            } else if (wheel) {
                *wheel = 0;
            }
            return ret;
        }
        return -1;
    }

    /* Report Protocol: Use parser */
    if (!hid->parser) return -1;
    
    int32_t x_val = 0, y_val = 0, wheel_val = 0;
    uint8_t btn_val = 0;
    
    if (hid->parser->parse_mouse_report((const uint8_t*)hid->report_buffer, ret,
                                        &x_val, &y_val, &btn_val, &wheel_val) < 0) {
        return -1;
    }

    /* Convert to int8_t */
    *dx = (int8_t)x_val;
    *dy = (int8_t)y_val;
    *buttons = btn_val;
    if (wheel) {
        *wheel = (int8_t)wheel_val;
    }

    return ret;
}

/**
 * Initialize HID keyboard
 */
int usb_hid_keyboard_init(usb_device_t *dev) {
    if (!dev) return -1;

    usb_hid_device_t *hid = (usb_hid_device_t *)kmalloc(sizeof(usb_hid_device_t));
    if (!hid) {
        return -1;
    }

    k_memset(hid, 0, sizeof(usb_hid_device_t));
    hid->device = dev;
    hid->is_keyboard = true;
    hid->protocol = HID_PROTOCOL_BOOT; /* Start with boot protocol */
    dev->driver_data = hid;

    /* Find interrupt IN endpoint */
    for (uint8_t i = 0; i < dev->num_endpoints; i++) {
        usb_endpoint_t *ep = &dev->endpoints[i];
        if ((ep->address & USB_ENDPOINT_DIR_IN) != 0 && ep->type == USB_ENDPOINT_XFER_INT) {
            hid->intr_in_ep = ep;
            klog_printf(KLOG_INFO, "usb_hid: keyboard interrupt endpoint 0x%02x", ep->address);
            break;
        }
    }

    if (!hid->intr_in_ep) {
        klog_printf(KLOG_ERROR, "usb_hid: keyboard has no interrupt IN endpoint");
        kfree(hid);
        return -1;
    }

    /* Initialize HID parser */
    hid->parser = new HIDParser(dev);
    if (!hid->parser) {
        klog_printf(KLOG_ERROR, "usb_hid: failed to allocate parser");
        kfree(hid);
        return -1;
    }

    if (hid->parser->init() < 0) {
        klog_printf(KLOG_ERROR, "usb_hid: failed to initialize parser");
        delete hid->parser;
        kfree(hid);
        return -1;
    }

    /* Get report size */
    hid->report_size = hid->parser->get_input_report_size(0);
    if (hid->report_size == 0) {
        hid->report_size = 8; /* Boot protocol keyboard: 8 bytes */
    }
    hid->report_buffer = kmalloc(hid->report_size);
    if (!hid->report_buffer) {
        delete hid->parser;
        kfree(hid);
        return -1;
    }

    /* HID Boot Protocol Setup Sequence */
    /* Step 1: Set IDLE (disable automatic reports) */
    usb_control_transfer(dev, USB_REQ_TYPE_CLASS | USB_REQ_TYPE_INTERFACE | USB_ENDPOINT_DIR_OUT,
                        HID_REQ_SET_IDLE, 0, 0, NULL, 0, 1000);
    
    /* Step 2: Set Boot Protocol (0) for simple keyboard format */
    int ret = usb_control_transfer(dev, USB_REQ_TYPE_CLASS | USB_REQ_TYPE_INTERFACE | USB_ENDPOINT_DIR_OUT,
                                   HID_REQ_SET_PROTOCOL, HID_PROTOCOL_BOOT, 0, NULL, 0, 1000);
    if (ret < 0) {
        klog_printf(KLOG_WARN, "usb_hid: failed to set boot protocol, trying report protocol");
        /* Fallback to report protocol */
        hid->protocol = HID_PROTOCOL_REPORT;
        usb_control_transfer(dev, USB_REQ_TYPE_CLASS | USB_REQ_TYPE_INTERFACE | USB_ENDPOINT_DIR_OUT,
                            HID_REQ_SET_PROTOCOL, HID_PROTOCOL_REPORT, 0, NULL, 0, 1000);
    } else {
        hid->protocol = HID_PROTOCOL_BOOT;
        klog_printf(KLOG_INFO, "usb_hid: keyboard set to boot protocol");
    }

    /* Register input device */
    static char kbd_dev_name[64];
    /* Use static buffer for device name (input_core stores pointer) */
    k_memset(kbd_dev_name, 0, sizeof(kbd_dev_name));
    /* Format: "USB Keyboard VID:PID" - simple hex formatting */
    char *p = kbd_dev_name;
    memcpy(p, "USB Keyboard ", 13);
    p += 13;
    /* Format VID */
    uint16_t vid = dev->vendor_id;
    for (int i = 0; i < 4; i++) {
        uint8_t nibble = (vid >> (12 - i * 4)) & 0xF;
        *p++ = (nibble < 10) ? ('0' + nibble) : ('A' + nibble - 10);
    }
    *p++ = ':';
    /* Format PID */
    uint16_t pid = dev->product_id;
    for (int i = 0; i < 4; i++) {
        uint8_t nibble = (pid >> (12 - i * 4)) & 0xF;
        *p++ = (nibble < 10) ? ('0' + nibble) : ('A' + nibble - 10);
    }
    *p = '\0';
    hid->input_dev = input_device_register(INPUT_DEVICE_KEYBOARD, kbd_dev_name);
    if (!hid->input_dev) {
        klog_printf(KLOG_ERROR, "usb_hid: failed to register keyboard input device");
        kfree(hid->report_buffer);
        delete hid->parser;
        kfree(hid);
        return -1;
    }
    hid->input_dev->driver_data = hid;
    
    /* Initialize last state */
    k_memset(hid->last_keys, 0, sizeof(hid->last_keys));
    hid->last_modifiers = 0;

    klog_printf(KLOG_INFO, "usb_hid: keyboard initialized (protocol=%u, report_size=%zu)", 
                hid->protocol, hid->report_size);
    return 0;
}

/**
 * Read keyboard data using HID parser or boot protocol
 */
int usb_hid_keyboard_read(usb_device_t *dev, uint8_t *modifiers, uint8_t *keys) {
    if (!dev || !modifiers || !keys) return -1;

    usb_hid_device_t *hid = (usb_hid_device_t *)dev->driver_data;
    if (!hid || !hid->is_keyboard || !hid->intr_in_ep) return -1;

    /* Read report (short timeout for non-blocking) */
    int ret = usb_interrupt_transfer(dev, hid->intr_in_ep, hid->report_buffer, hid->report_size, 10);
    if (ret < 0) {
        return -1;
    }

    /* Boot Protocol: 8 bytes [modifiers, reserved, key[6]] */
    if (hid->protocol == HID_PROTOCOL_BOOT) {
        if (ret >= 2) {
            uint8_t *buf = (uint8_t*)hid->report_buffer;
            *modifiers = buf[0];
            /* Boot protocol: keys start at offset 2 */
            if (ret >= 8) {
                memcpy(keys, &buf[2], 6);
            } else {
                k_memset(keys, 0, 6);
                if (ret > 2) {
                    memcpy(keys, &buf[2], ret - 2);
                }
            }
            return ret;
        }
        return -1;
    }

    /* Report Protocol: Use parser */
    if (!hid->parser) return -1;
    
    uint8_t mod_val = 0;
    uint8_t key_array[6] = {0};
    
    if (hid->parser->parse_keyboard_report((const uint8_t*)hid->report_buffer, ret,
                                           &mod_val, key_array, 6) < 0) {
        return -1;
    }

    *modifiers = mod_val;
    memcpy(keys, key_array, 6);
    return ret;
}

/**
 * Process keyboard report and generate input events
 */
void usb_hid_process_keyboard_report(usb_device_t *dev, uint8_t *report, size_t len) {
    if (!dev) return;
    
    usb_hid_device_t *hid = (usb_hid_device_t *)dev->driver_data;
    if (!hid || !hid->is_keyboard || !hid->input_dev) return;
    
    uint8_t modifiers = 0;
    uint8_t keys[6] = {0};
    
    /* Parse report */
    if (usb_hid_keyboard_read(dev, &modifiers, keys) < 0) {
        return;
    }
    
    /* Detect key releases (keys that were pressed but are now released) */
    for (int i = 0; i < 6; i++) {
        uint8_t last_key = hid->last_keys[i];
        if (last_key != 0) {
            /* Check if this key is still pressed */
            bool still_pressed = false;
            for (int j = 0; j < 6; j++) {
                if (keys[j] == last_key) {
                    still_pressed = true;
                    break;
                }
            }
            if (!still_pressed) {
                /* Key was released */
                input_key_release(hid->input_dev, last_key);
            }
        }
    }
    
    /* Detect key presses (new keys that weren't pressed before) */
    for (int i = 0; i < 6; i++) {
        if (keys[i] == 0) continue;
        
        /* Check if this key was already pressed */
        bool was_pressed = false;
        for (int j = 0; j < 6; j++) {
            if (hid->last_keys[j] == keys[i]) {
                was_pressed = true;
                break;
            }
        }
        
        if (!was_pressed) {
            /* New key press */
            input_key_press(hid->input_dev, keys[i], modifiers);
            
            /* Convert to ASCII if possible (for simple keys) */
            if (keys[i] >= 0x04 && keys[i] <= 0x1D) {
                /* Letters a-z */
                char ch = 'a' + (keys[i] - 0x04);
                if (modifiers & 0x22) { /* Shift */
                    ch = ch - 'a' + 'A';
                }
                input_key_char(hid->input_dev, ch);
            } else if (keys[i] >= 0x1E && keys[i] <= 0x27) {
                /* Numbers 1-9, 0 */
                char ch = (keys[i] == 0x27) ? '0' : ('1' + (keys[i] - 0x1E));
                input_key_char(hid->input_dev, ch);
            } else if (keys[i] == 0x28) {
                /* Enter */
                input_key_char(hid->input_dev, '\n');
            } else if (keys[i] == 0x2C) {
                /* Space */
                input_key_char(hid->input_dev, ' ');
            } else if (keys[i] == 0x2D) {
                /* Minus */
                input_key_char(hid->input_dev, '-');
            } else if (keys[i] == 0x2E) {
                /* Equals */
                input_key_char(hid->input_dev, '=');
            } else if (keys[i] == 0x2F) {
                /* Left bracket */
                input_key_char(hid->input_dev, '[');
            } else if (keys[i] == 0x30) {
                /* Right bracket */
                input_key_char(hid->input_dev, ']');
            } else if (keys[i] == 0x31) {
                /* Backslash */
                input_key_char(hid->input_dev, '\\');
            } else if (keys[i] == 0x33) {
                /* Semicolon */
                input_key_char(hid->input_dev, ';');
            } else if (keys[i] == 0x34) {
                /* Quote */
                input_key_char(hid->input_dev, '\'');
            } else if (keys[i] == 0x35) {
                /* Grave */
                input_key_char(hid->input_dev, '`');
            } else if (keys[i] == 0x36) {
                /* Comma */
                input_key_char(hid->input_dev, ',');
            } else if (keys[i] == 0x37) {
                /* Period */
                input_key_char(hid->input_dev, '.');
            } else if (keys[i] == 0x38) {
                /* Slash */
                input_key_char(hid->input_dev, '/');
            }
        }
    }
    
    /* Update last state */
    memcpy(hid->last_keys, keys, 6);
    hid->last_modifiers = modifiers;
}

/**
 * Process mouse report and generate input events
 */
void usb_hid_process_mouse_report(usb_device_t *dev, uint8_t *report, size_t len) {
    (void)report; /* Report is read via usb_hid_mouse_read */
    (void)len;
    
    if (!dev) return;
    
    usb_hid_device_t *hid = (usb_hid_device_t *)dev->driver_data;
    if (!hid || !hid->is_mouse || !hid->input_dev) return;
    
    int8_t dx = 0, dy = 0, wheel = 0;
    uint8_t buttons = 0;
    
    /* Parse report */
    if (usb_hid_mouse_read(dev, &dx, &dy, &buttons, &wheel) < 0) {
        return;
    }
    
    /* Generate mouse move event */
    if (dx != 0 || dy != 0) {
        input_mouse_move(hid->input_dev, 0, 0, dx, dy, buttons);
    }
    
    /* Generate scroll event */
    if (wheel != 0) {
        input_mouse_scroll(hid->input_dev, wheel);
    }
    
    /* Generate button events (detect button state changes) */
    static uint8_t last_buttons = 0;
    uint8_t changed = buttons ^ last_buttons;
    if (changed & INPUT_MOUSE_BUTTON_LEFT) {
        input_mouse_button(hid->input_dev, INPUT_MOUSE_BUTTON_LEFT, 
                          (buttons & INPUT_MOUSE_BUTTON_LEFT) != 0);
    }
    if (changed & INPUT_MOUSE_BUTTON_RIGHT) {
        input_mouse_button(hid->input_dev, INPUT_MOUSE_BUTTON_RIGHT,
                          (buttons & INPUT_MOUSE_BUTTON_RIGHT) != 0);
    }
    if (changed & INPUT_MOUSE_BUTTON_MIDDLE) {
        input_mouse_button(hid->input_dev, INPUT_MOUSE_BUTTON_MIDDLE,
                          (buttons & INPUT_MOUSE_BUTTON_MIDDLE) != 0);
    }
    last_buttons = buttons;
}

