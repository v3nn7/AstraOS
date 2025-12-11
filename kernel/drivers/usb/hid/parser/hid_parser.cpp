/**
 * HID Parser Implementation
 * 
 * High-level interface for parsing HID devices and extracting report data.
 */

#include <drivers/usb/hid/hid_parser.hpp>
#include <drivers/usb/usb_descriptors.h>
#include <drivers/usb/usb_transfer.h>
#include "kmalloc.h"
#include "klog.h"
#include "string.h"

/* Helper function to extract array element value from HID report */
static uint32_t extract_array_element(const uint8_t* report, size_t report_size,
                                       uint32_t element_offset, uint32_t element_size) {
    if (element_offset + element_size > report_size * 8) {
        return 0;
    }
    
    uint32_t value = 0;
    uint32_t bit_offset = element_offset;
    uint32_t bits_remaining = element_size;
    
    while (bits_remaining > 0 && bit_offset < report_size * 8) {
        uint32_t byte_idx = bit_offset / 8;
        uint32_t bit_idx = bit_offset % 8;
        uint32_t bits_to_read = (bits_remaining < 8) ? bits_remaining : 8;
        
        if (byte_idx >= report_size) break;
        
        uint8_t byte = report[byte_idx];
        byte >>= bit_idx;
        byte &= (1 << bits_to_read) - 1;
        
        value |= ((uint32_t)byte) << (element_size - bits_remaining);
        
        bit_offset += bits_to_read;
        bits_remaining -= bits_to_read;
    }
    
    return value;
}

HIDParser::HIDParser(usb_device_t* dev, uint8_t interface) :
    report_descriptor(nullptr),
    device(dev),
    report_buffer(nullptr),
    report_buffer_size(0),
    cached_mouse_x(nullptr),
    cached_mouse_y(nullptr),
    cached_mouse_wheel(nullptr),
    cached_keyboard_modifiers(nullptr),
    cached_keyboard_keys(nullptr),
    interface_number(interface) {
    /* Initialize button cache array */
    for (int i = 0; i < 5; i++) {
        cached_mouse_buttons[i] = nullptr;
    }
}

HIDParser::~HIDParser() {
    if (report_descriptor) {
        delete report_descriptor;
    }
    if (report_buffer) {
        kfree(report_buffer);
    }
}

int HIDParser::init() {
    if (!device) {
        return -1;
    }
    
    /* Get HID descriptor */
    usb_hid_descriptor_t hid_desc;
    int ret = usb_get_descriptor(device, USB_DT_HID, 0, 0, &hid_desc, sizeof(hid_desc));
    if (ret < 0) {
        klog_printf(KLOG_ERROR, "hid_parser: failed to get HID descriptor");
        return -1;
    }
    
    /* Get report descriptor */
    uint16_t desc_length = hid_desc.wDescriptorLength;
    if (desc_length == 0) {
        desc_length = 256; /* Default size */
    }
    
    uint8_t* desc_data = (uint8_t*)kmalloc(desc_length);
    if (!desc_data) {
        return -1;
    }
    
    ret = usb_control_transfer(device, 
                               USB_ENDPOINT_DIR_IN | USB_REQ_TYPE_STANDARD | USB_REQ_TYPE_INTERFACE,
                               USB_REQ_GET_DESCRIPTOR,
                               (USB_DT_HID_REPORT << 8) | 0,
                               interface_number, /* Use interface_number */
                               desc_data,
                               desc_length,
                               1000);
    
    if (ret < 0) {
        klog_printf(KLOG_ERROR, "hid_parser: failed to get report descriptor");
        kfree(desc_data);
        return -1;
    }
    
    /* Parse report descriptor */
    report_descriptor = new HIDReportDescriptor();
    if (!report_descriptor) {
        kfree(desc_data);
        return -1;
    }
    
    ret = report_descriptor->parse(desc_data, desc_length);
    kfree(desc_data);
    
    if (ret < 0) {
        klog_printf(KLOG_ERROR, "hid_parser: failed to parse report descriptor");
        delete report_descriptor;
        report_descriptor = nullptr;
        return -1;
    }
    
    /* Allocate report buffer */
    report_buffer_size = report_descriptor->get_report_size(1, 0); /* Input = 1, report_id = 0 */
    if (report_buffer_size == 0) {
        report_buffer_size = 64; /* Default size */
    }
    report_buffer = (uint8_t*)kmalloc(report_buffer_size);
    if (!report_buffer) {
        return -1;
    }
    
    /* Cache field pointers for performance (avoid searching on every report) */
    HIDUsage x_usage(HID_USAGE_PAGE_GENERIC_DESKTOP, HID_USAGE_GD_X);
    cached_mouse_x = report_descriptor->find_field(x_usage, 1); /* Input */
    
    HIDUsage y_usage(HID_USAGE_PAGE_GENERIC_DESKTOP, HID_USAGE_GD_Y);
    cached_mouse_y = report_descriptor->find_field(y_usage, 1);
    
    HIDUsage wheel_usage(HID_USAGE_PAGE_GENERIC_DESKTOP, HID_USAGE_GD_WHEEL);
    cached_mouse_wheel = report_descriptor->find_field(wheel_usage, 1);
    
    /* Cache button fields */
    for (uint8_t i = 1; i <= 5; i++) {
        HIDUsage btn_usage(HID_USAGE_PAGE_BUTTON, i);
        cached_mouse_buttons[i - 1] = report_descriptor->find_field(btn_usage, 1);
    }
    
    /* Cache keyboard modifier field (Usage Page 0x07, Usage ID 0xE0-0xE7) */
    uint32_t field_count = 0;
    HIDReportField* fields = report_descriptor->get_fields(1, &field_count); /* Input */
    if (fields && field_count > 0) {
        for (uint32_t i = 0; i < field_count; i++) {
            /* Look for modifier keys: Usage Page Keyboard (0x07), Usage 0xE0-0xE7 */
            if (fields[i].usage_min.page == HID_USAGE_PAGE_KEYBOARD &&
                fields[i].usage_min.usage >= 0xE0 && fields[i].usage_min.usage <= 0xE7) {
                cached_keyboard_modifiers = &fields[i];
                break;
            }
        }
        
        /* Cache key array field (Usage Page Keyboard, Usage 0x04-0xA5) */
        for (uint32_t i = 0; i < field_count; i++) {
            if (fields[i].usage_min.page == HID_USAGE_PAGE_KEYBOARD &&
                fields[i].usage_min.usage >= 0x04 && fields[i].usage_min.usage <= 0xA5) {
                cached_keyboard_keys = &fields[i];
                break;
            }
        }
    }
    
    klog_printf(KLOG_INFO, "hid_parser: initialized, report size = %zu bytes, cached %u fields",
                report_buffer_size, field_count);
    
    return 0;
}

int HIDParser::parse_mouse_report(const uint8_t* report, size_t size,
                                   int32_t* dx, int32_t* dy, uint8_t* buttons, int32_t* wheel) {
    if (!report_descriptor || !report) {
        return -1;
    }
    
    if (dx) *dx = 0;
    if (dy) *dy = 0;
    if (buttons) *buttons = 0;
    if (wheel) *wheel = 0;
    
    /* Use cached field pointers (no search overhead) */
    if (cached_mouse_x) {
        int32_t val = cached_mouse_x->get_value(report, size);
        if (dx) *dx = val;
    }
    
    if (cached_mouse_y) {
        int32_t val = cached_mouse_y->get_value(report, size);
        if (dy) *dy = val;
    }
    
    if (cached_mouse_wheel) {
        int32_t val = cached_mouse_wheel->get_value(report, size);
        if (wheel) *wheel = val;
    }
    
    /* Use cached button fields */
    uint8_t button_bits = 0;
    for (uint8_t i = 0; i < 5; i++) {
        if (cached_mouse_buttons[i]) {
            int32_t val = cached_mouse_buttons[i]->get_value(report, size);
            if (val) {
                button_bits |= (1 << i);
            }
        }
    }
    if (buttons) *buttons = button_bits;
    
    return 0;
}

int HIDParser::parse_keyboard_report(const uint8_t* report, size_t size,
                                      uint8_t* modifiers, uint8_t* keys, uint8_t max_keys) {
    if (!report_descriptor || !report) {
        return -1;
    }
    
    if (modifiers) *modifiers = 0;
    if (keys) {
        for (uint8_t i = 0; i < max_keys; i++) {
            keys[i] = 0;
        }
    }
    
    /* Use cached modifier field (Usage Page 0x07, Usage 0xE0-0xE7) */
    if (cached_keyboard_modifiers) {
        /* Extract modifier bits from the field (not hardcoded report[0]) */
        int32_t mod_value = cached_keyboard_modifiers->get_value(report, size);
        if (modifiers) {
            *modifiers = (uint8_t)mod_value;
        }
    } else {
        /* Fallback: if no cached field, try to find it dynamically */
        uint32_t field_count = 0;
        HIDReportField* fields = report_descriptor->get_fields(1, &field_count); /* Input */
        if (fields && field_count > 0) {
            for (uint32_t i = 0; i < field_count; i++) {
                if (fields[i].usage_min.page == HID_USAGE_PAGE_KEYBOARD &&
                    fields[i].usage_min.usage >= 0xE0 && fields[i].usage_min.usage <= 0xE7) {
                    int32_t mod_value = fields[i].get_value(report, size);
                    if (modifiers) {
                        *modifiers = (uint8_t)mod_value;
                    }
                    break;
                }
            }
        }
    }
    
    /* Use cached key array field (Usage Page Keyboard, Usage 0x04-0xA5) */
    if (cached_keyboard_keys && keys) {
        uint8_t key_idx = 0;
        /* Key array fields typically have count > 1 (multiple keys can be pressed) */
        for (uint32_t j = 0; j < cached_keyboard_keys->count && key_idx < max_keys; j++) {
            /* Extract array element value */
            uint32_t element_offset = cached_keyboard_keys->offset + (j * cached_keyboard_keys->size);
            uint32_t value = extract_array_element(report, size, element_offset, cached_keyboard_keys->size);
            
            /* Filter valid key codes (0x04-0xA5) and non-zero values */
            if (value != 0 && value >= 0x04 && value <= 0xA5) {
                keys[key_idx++] = (uint8_t)value;
            }
        }
    } else {
        /* Fallback: if no cached field, try to find it dynamically */
        uint32_t field_count = 0;
        HIDReportField* fields = report_descriptor->get_fields(1, &field_count); /* Input */
        if (fields && field_count > 0 && keys) {
            uint8_t key_idx = 0;
            for (uint32_t i = 0; i < field_count && key_idx < max_keys; i++) {
                if (fields[i].usage_min.page == HID_USAGE_PAGE_KEYBOARD &&
                    fields[i].usage_min.usage >= 0x04 && fields[i].usage_min.usage <= 0xA5) {
                    /* Extract keys from array field */
                    for (uint32_t j = 0; j < fields[i].count && key_idx < max_keys; j++) {
                        uint32_t element_offset = fields[i].offset + (j * fields[i].size);
                        uint32_t value = extract_array_element(report, size, element_offset, fields[i].size);
                        
                        if (value != 0 && value >= 0x04 && value <= 0xA5) {
                            keys[key_idx++] = (uint8_t)value;
                        }
                    }
                    break;
                }
            }
        }
    }
    
    return 0;
}

size_t HIDParser::get_input_report_size(uint8_t report_id) {
    if (!report_descriptor) {
        return 0;
    }
    return report_descriptor->get_report_size(1, report_id); /* Input = 1 */
}

