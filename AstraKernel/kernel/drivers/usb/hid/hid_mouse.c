/**
 * USB HID Mouse Integration
 * 
 * Integrates USB HID mouse with AstraOS event system
 * and provides polling functions for shell/main loop.
 */

#include "usb/usb_hid.h"
#include "usb/usb_core.h"
#include "usb/usb_device.h"
#include "usb/usb_transfer.h"
#include "event.h"
#include "kernel.h"
#include "klog.h"

/* USB HID Mouse State */
static usb_device_t *usb_hid_mouse_device = NULL;
static int usb_mouse_x = 0;
static int usb_mouse_y = 0;
static uint32_t screen_w = 0;
static uint32_t screen_h = 0;

/* Forward declaration */
extern usb_device_t *usb_hid_keyboard_device;

/**
 * Find and initialize USB HID devices
 */
void usb_hid_scan_devices(void) {
    klog_printf(KLOG_INFO, "usb_hid: scanning USB devices for HID class...");
    
    /* Iterate through all USB devices */
    /* We'll scan addresses 1-127 */
    uint32_t devices_checked = 0;
    for (uint8_t addr = 1; addr < 128; addr++) {
        usb_device_t *dev = usb_device_find_by_address(addr);
        if (!dev) continue;
        
        devices_checked++;
        klog_printf(KLOG_INFO, "usb_hid: checking device at address %d (class=0x%02x, VID:PID=%04x:%04x)",
                    addr, dev->device_class, dev->vendor_id, dev->product_id);
        
        /* Check if device is HID class */
        if (dev->device_class == 0x03) { /* HID Class */
            klog_printf(KLOG_INFO, "usb_hid: found HID device VID:PID=%04x:%04x at address %d",
                        dev->vendor_id, dev->product_id, addr);
            
            /* Try to probe device */
            if (usb_hid_probe_device(dev) == 0) {
                /* Check if it's a mouse or keyboard by checking endpoints */
                bool has_intr_in = false;
                for (uint8_t i = 0; i < dev->num_endpoints; i++) {
                    usb_endpoint_t *ep = &dev->endpoints[i];
                    if ((ep->address & USB_ENDPOINT_DIR_IN) != 0 && 
                        ep->type == USB_ENDPOINT_XFER_INT) {
                        has_intr_in = true;
                        break;
                    }
                }
                
                if (has_intr_in) {
                    /* Try to initialize as mouse first */
                    if (!usb_hid_mouse_device) {
                        usb_hid_register_mouse(dev);
                    }
                    /* If mouse registration failed or already exists, try keyboard */
                    if (!usb_hid_keyboard_device && dev != usb_hid_mouse_device) {
                        usb_hid_register_keyboard(dev);
                    }
                }
            }
        }
    }
    
    klog_printf(KLOG_INFO, "usb_hid: scan complete (checked %u devices, mouse=%s, keyboard=%s)",
                devices_checked,
                usb_hid_mouse_device ? "yes" : "no",
                usb_hid_keyboard_device ? "yes" : "no");
}

/**
 * Register USB HID mouse device
 */
void usb_hid_register_mouse(usb_device_t *dev) {
    if (!dev) return;
    
    if (usb_hid_mouse_init(dev) == 0) {
        usb_hid_mouse_device = dev;
        extern uint32_t fb_width(void);
        extern uint32_t fb_height(void);
        screen_w = fb_width();
        screen_h = fb_height();
        if (screen_w == 0 || screen_h == 0) {
            screen_w = 800;
            screen_h = 600;
        }
        usb_mouse_x = screen_w / 2;
        usb_mouse_y = screen_h / 2;
        klog_printf(KLOG_INFO, "usb_hid: registered mouse device");
    }
}

/**
 * Poll USB HID mouse and update events
 */
void usb_hid_poll_mouse(void) {
    if (!usb_hid_mouse_device) return;

    /* Process mouse report and generate input events */
    extern void usb_hid_process_mouse_report(usb_device_t *dev, uint8_t *report, size_t len);
    
    /* Read report - this will populate the report buffer */
    int8_t dx, dy, wheel = 0;
    uint8_t buttons;
    
    int ret = usb_hid_mouse_read(usb_hid_mouse_device, &dx, &dy, &buttons, &wheel);
    if (ret < 0) {
        return; /* No data or error */
    }
    
    /* Get report buffer from device driver data */
    typedef struct {
        void *report_buffer;
        size_t report_size;
    } usb_hid_device_t;
    usb_hid_device_t *hid = (usb_hid_device_t *)usb_hid_mouse_device->driver_data;
    if (hid && hid->report_buffer) {
        /* Process the report that was just read */
        usb_hid_process_mouse_report(usb_hid_mouse_device, 
                                     (uint8_t*)hid->report_buffer, 
                                     ret);
    }

    /* Update position for GUI (legacy support) */
    usb_mouse_x += dx;
    usb_mouse_y -= dy; /* Invert Y axis */

    /* Clamp to screen bounds */
    extern uint32_t fb_width(void);
    extern uint32_t fb_height(void);
    uint32_t w = fb_width();
    uint32_t h = fb_height();
    if (w == 0) w = screen_w;
    if (h == 0) h = screen_h;
    
    if (usb_mouse_x < 0) usb_mouse_x = 0;
    if (usb_mouse_y < 0) usb_mouse_y = 0;
    if (usb_mouse_x >= (int)w) usb_mouse_x = w - 1;
    if (usb_mouse_y >= (int)h) usb_mouse_y = h - 1;

    /* Push mouse move event (legacy GUI support) */
    gui_event_push_mouse_move(usb_mouse_x, usb_mouse_y, dx, -dy, buttons);

    /* Push scroll event if wheel moved (legacy GUI support) */
    if (wheel != 0) {
        gui_event_push_mouse_scroll(usb_mouse_x, usb_mouse_y, wheel);
    }
}

/**
 * Get USB mouse X position
 */
int usb_hid_mouse_get_x(void) {
    return usb_mouse_x;
}

/**
 * Get USB mouse Y position
 */
int usb_hid_mouse_get_y(void) {
    return usb_mouse_y;
}

/**
 * Check if USB HID mouse is available
 */
bool usb_hid_mouse_available(void) {
    return usb_hid_mouse_device != NULL;
}

/**
 * Mouse init wrapper (for drivers.h compatibility)
 */
void mouse_init(void) {
    /* USB HID mouse is initialized via usb_hid_init() */
    /* This function is called from kmain() */
}

/**
 * Get mouse X position
 */
int mouse_get_x(void) {
    return usb_hid_mouse_get_x();
}

/**
 * Get mouse Y position
 */
int mouse_get_y(void) {
    return usb_hid_mouse_get_y();
}

/**
 * Check if mouse cursor needs redraw
 */
bool mouse_cursor_needs_redraw(void) {
    /* Always return true for now - cursor should be redrawn every frame */
    return true;
}

/**
 * Update mouse cursor
 */
void mouse_cursor_update(void) {
    extern void mouse_cursor_draw(int x, int y);
    int x = mouse_get_x();
    int y = mouse_get_y();
    if (x >= 0 && y >= 0) {
        mouse_cursor_draw(x, y);
    }
}


