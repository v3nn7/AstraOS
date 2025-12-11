#pragma once

#include "types.h"
#include <drivers/usb/hid/hid_report_descriptor.hpp>
#include <drivers/usb/usb_core.h>

/**
 * HID Parser
 * 
 * High-level interface for parsing HID devices and extracting report data.
 */

class HIDParser {
private:
    HIDReportDescriptor* report_descriptor;
    usb_device_t* device;
    uint8_t* report_buffer;
    size_t report_buffer_size;
    
    /* Cached field pointers for performance (avoid searching on every report) */
    HIDReportField* cached_mouse_x;
    HIDReportField* cached_mouse_y;
    HIDReportField* cached_mouse_wheel;
    HIDReportField* cached_mouse_buttons[5]; /* Buttons 1-5 */
    HIDReportField* cached_keyboard_modifiers; /* Modifier keys field */
    HIDReportField* cached_keyboard_keys; /* Key array field */
    
    /* Interface number (for future multi-interface support) */
    uint8_t interface_number;
    
public:
    HIDParser(usb_device_t* dev, uint8_t interface = 0);
    ~HIDParser();
    
    /* Initialize parser with device's HID descriptor */
    int init();
    
    /* Parse mouse report */
    int parse_mouse_report(const uint8_t* report, size_t size,
                          int32_t* dx, int32_t* dy, uint8_t* buttons, int32_t* wheel);
    
    /* Parse keyboard report */
    int parse_keyboard_report(const uint8_t* report, size_t size,
                             uint8_t* modifiers, uint8_t* keys, uint8_t max_keys);
    
    /* Get report size */
    size_t get_input_report_size(uint8_t report_id = 0);
    
    /* Get report descriptor */
    HIDReportDescriptor* get_report_descriptor() { return report_descriptor; }
};

