#pragma once

#include "types.h"
#include "usb/hid/hid_usage.hpp"

/**
 * HID Report Field
 * 
 * Represents a single field in a HID report (e.g., X axis, Button 1, etc.)
 */

/* HID Field Flags */
#define HID_FIELD_DATA           0x00
#define HID_FIELD_CONSTANT       0x01
#define HID_FIELD_ARRAY          0x00
#define HID_FIELD_VARIABLE       0x02
#define HID_FIELD_ABSOLUTE       0x00
#define HID_FIELD_RELATIVE       0x04
#define HID_FIELD_NO_WRAP        0x00
#define HID_FIELD_WRAP           0x08
#define HID_FIELD_LINEAR         0x00
#define HID_FIELD_NONLINEAR      0x10
#define HID_FIELD_PREFERRED      0x00
#define HID_FIELD_NO_PREFERRED   0x20
#define HID_FIELD_NO_NULL        0x00
#define HID_FIELD_NULL_STATE     0x40
#define HID_FIELD_NON_VOLATILE   0x00
#define HID_FIELD_VOLATILE       0x80
#define HID_FIELD_BITFIELD       0x00
#define HID_FIELD_BUFFERED       0x100

class HIDReportField {
public:
    HIDUsage usage_min;
    HIDUsage usage_max;
    uint32_t logical_min;
    uint32_t logical_max;
    uint32_t physical_min;
    uint32_t physical_max;
    uint32_t unit;
    uint32_t unit_exponent;
    uint32_t size;          /* Size in bits */
    uint32_t count;         /* Number of usages */
    uint32_t offset;        /* Bit offset in report */
    uint32_t flags;         /* Field flags */
    uint8_t report_id;      /* Report ID */
    uint8_t report_type;    /* Input/Output/Feature */
    
    HIDReportField() :
        logical_min(0), logical_max(0),
        physical_min(0), physical_max(0),
        unit(0), unit_exponent(0),
        size(0), count(0), offset(0), flags(0),
        report_id(0), report_type(0) {}
    
    /* Check if field matches a usage */
    bool matches_usage(const HIDUsage& u) const {
        if (usage_min.page != u.page) return false;
        if (count == 1) {
            return usage_min.usage == u.usage;
        }
        return u.usage >= usage_min.usage && u.usage <= usage_max.usage;
    }
    
    /* Get value from report buffer */
    int32_t get_value(const uint8_t* report, size_t report_size) const {
        if (offset + size > report_size * 8) return 0;
        
        uint32_t value = 0;
        uint32_t bit_offset = offset;
        uint32_t bits_remaining = size;
        
        /* Extract bits from report */
        while (bits_remaining > 0) {
            uint32_t byte_idx = bit_offset / 8;
            uint32_t bit_idx = bit_offset % 8;
            uint32_t bits_to_read = (bits_remaining < 8) ? bits_remaining : 8;
            
            if (byte_idx >= report_size) break;
            
            uint8_t byte = report[byte_idx];
            byte >>= bit_idx;
            byte &= (1 << bits_to_read) - 1;
            
            value |= ((uint32_t)byte) << (size - bits_remaining);
            
            bit_offset += bits_to_read;
            bits_remaining -= bits_to_read;
        }
        
        /* Sign extend if signed (logical_min <= max signed value for size) */
        if (flags & HID_FIELD_DATA && size > 0) {
            uint32_t max_signed = (1U << (size - 1)) - 1;
            if (logical_min <= max_signed && (value & (1U << (size - 1)))) {
                value |= ~((1U << size) - 1);
            }
        }
        
        return (int32_t)value;
    }
    
    /* Set value in report buffer */
    void set_value(uint8_t* report, size_t report_size, int32_t value) const {
        if (offset + size > report_size * 8) return;
        if (report_type != 2 && report_type != 3) return; /* Output or Feature */
        
        uint32_t bit_offset = offset;
        uint32_t bits_remaining = size;
        uint32_t val = (uint32_t)value;
        
        /* Insert bits into report */
        while (bits_remaining > 0) {
            uint32_t byte_idx = bit_offset / 8;
            uint32_t bit_idx = bit_offset % 8;
            uint32_t bits_to_write = (bits_remaining < 8) ? bits_remaining : 8;
            
            if (byte_idx >= report_size) break;
            
            uint8_t mask = ((1 << bits_to_write) - 1) << bit_idx;
            uint8_t bits = (val >> (size - bits_remaining)) & ((1 << bits_to_write) - 1);
            
            report[byte_idx] = (report[byte_idx] & ~mask) | (bits << bit_idx);
            
            bit_offset += bits_to_write;
            bits_remaining -= bits_to_write;
        }
    }
};

