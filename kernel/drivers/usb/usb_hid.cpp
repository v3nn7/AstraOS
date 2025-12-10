// Generic USB HID parsing helpers.
#include "usb_hid.hpp"

namespace usb {

bool hid_parse_descriptor(const uint8_t* data, size_t len, HidDescriptor* out) {
    if (!data || len < sizeof(HidDescriptor) || !out) {
        return false;
    }
    const auto* desc = reinterpret_cast<const HidDescriptor*>(data);
    *out = *desc;
    return desc->descriptor_type == 0x21;
}

bool hid_parse_report(const uint8_t* data, size_t len, HidReportInfo* out) {
    // Minimal parser: detect a keyboard usage page (0x07) and usage (0x06, 0x07).
    if (!data || len == 0 || !out) {
        return false;
    }
    bool keyboard = false;
    uint8_t report_id = 0;
    for (size_t i = 0; i < len; ++i) {
        const uint8_t b = data[i];
        const uint8_t tag = b & 0xFC;
        const uint8_t size_code = b & 0x03;
        uint8_t item_size = 0;
        switch (size_code) {
            case 0: item_size = 0; break;
            case 1: item_size = 1; break;
            case 2: item_size = 2; break;
            case 3: item_size = 4; break;
        }
        if (tag == 0x04 && item_size >= 1 && i + 1 < len) {  // Usage Page
            uint8_t page = data[i + 1];
            if (page == 0x07) {
                keyboard = true;
            }
        }
        if (tag == 0x08 && item_size >= 1 && i + 1 < len) {  // Usage
            uint8_t usage = data[i + 1];
            if (usage == 0x06 || usage == 0x07) {
                keyboard = true;
            }
        }
        if (tag == 0x84 && item_size >= 1 && i + 1 < len) {  // Report ID (0x84 is 0b10000100)
            report_id = data[i + 1];
        }
        i += item_size;
    }
    out->has_keyboard_usage = keyboard;
    out->report_length = static_cast<uint16_t>(len);
    out->report_id = report_id;
    return keyboard;
}

}  // namespace usb
