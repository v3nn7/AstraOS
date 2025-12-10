// Generic USB HID parsing helpers.
#pragma once

#include <stdint.h>
#include <stddef.h>

namespace usb {

struct __attribute__((packed)) HidDescriptor {
    uint8_t length;
    uint8_t descriptor_type;  // 0x21
    uint16_t hid_bcd;
    uint8_t country_code;
    uint8_t num_descriptors;
    uint8_t report_type;
    uint16_t report_length;
};

struct HidReportInfo {
    uint16_t report_length;
    uint8_t report_id;
    bool has_keyboard_usage;
};

// Parse a HID descriptor and extract report length.
bool hid_parse_descriptor(const uint8_t* data, size_t len, HidDescriptor* out);

// Parse a minimal keyboard report descriptor and flag keyboard usage present.
bool hid_parse_report(const uint8_t* data, size_t len, HidReportInfo* out);

}  // namespace usb
