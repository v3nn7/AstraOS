#pragma once

#include "types.h"
#include <drivers/usb/hid/hid_field.hpp>
#include <drivers/usb/hid/hid_usage.hpp>

/**
 * HID Report Descriptor
 * 
 * Parsed representation of a HID report descriptor with all fields and collections.
 */

/* HID Item Types */
#define HID_ITEM_TYPE_MAIN         0x00
#define HID_ITEM_TYPE_GLOBAL       0x01
#define HID_ITEM_TYPE_LOCAL        0x02
#define HID_ITEM_TYPE_RESERVED     0x03

/* HID Main Items */
#define HID_MAIN_INPUT             0x80
#define HID_MAIN_OUTPUT            0x90
#define HID_MAIN_FEATURE           0xB0
#define HID_MAIN_COLLECTION        0xA0
#define HID_MAIN_END_COLLECTION    0xC0

/* HID Global Items */
#define HID_GLOBAL_USAGE_PAGE      0x00
#define HID_GLOBAL_LOGICAL_MIN     0x10
#define HID_GLOBAL_LOGICAL_MAX     0x20
#define HID_GLOBAL_PHYSICAL_MIN    0x30
#define HID_GLOBAL_PHYSICAL_MAX    0x40
#define HID_GLOBAL_UNIT_EXPONENT   0x50
#define HID_GLOBAL_UNIT            0x60
#define HID_GLOBAL_REPORT_SIZE     0x70
#define HID_GLOBAL_REPORT_ID       0x80
#define HID_GLOBAL_REPORT_COUNT    0x90
#define HID_GLOBAL_PUSH            0xA0
#define HID_GLOBAL_POP             0xB0

/* HID Local Items */
#define HID_LOCAL_USAGE            0x00
#define HID_LOCAL_USAGE_MIN        0x10
#define HID_LOCAL_USAGE_MAX        0x20
#define HID_LOCAL_DESIGNATOR_INDEX 0x30
#define HID_LOCAL_DESIGNATOR_MIN   0x40
#define HID_LOCAL_DESIGNATOR_MAX   0x50
#define HID_LOCAL_STRING_INDEX     0x70
#define HID_LOCAL_STRING_MIN       0x80
#define HID_LOCAL_STRING_MAX       0x90
#define HID_LOCAL_DELIMITER        0xA0

/* HID Collection Types */
#define HID_COLLECTION_PHYSICAL    0x00
#define HID_COLLECTION_APPLICATION 0x01
#define HID_COLLECTION_LOGICAL     0x02
#define HID_COLLECTION_REPORT      0x03
#define HID_COLLECTION_NAMED_ARRAY 0x04
#define HID_COLLECTION_USAGE_SWITCH 0x05
#define HID_COLLECTION_USAGE_MODIFIER 0x06

class HIDCollection {
public:
    uint8_t type;
    HIDUsage usage;
    HIDReportField* fields;
    uint32_t field_count;
    HIDCollection* children;
    uint32_t child_count;
    
    HIDCollection() :
        type(0), field_count(0), children(nullptr), child_count(0) {}
    
    ~HIDCollection() {
        if (fields) {
            delete[] fields;
        }
        if (children) {
            delete[] children;
        }
    }
};

class HIDReportDescriptor {
private:
    HIDCollection* root_collection;
    uint8_t* descriptor_data;
    size_t descriptor_size;
    
    /* Global state during parsing */
    uint16_t usage_page;
    int32_t logical_min;
    int32_t logical_max;
    int32_t physical_min;
    int32_t physical_max;
    uint32_t unit;
    uint32_t unit_exponent;
    uint32_t report_size;
    uint32_t report_count;
    uint8_t report_id;
    
    /* Local state */
    HIDUsage* usages;
    uint32_t usage_count;
    HIDUsage usage_min;
    HIDUsage usage_max;
    
    /* Report building state */
    uint32_t report_bit_offset[3]; /* Input, Output, Feature */
    HIDReportField* current_fields[3];
    uint32_t current_field_count[3];
    
    void reset_global_state();
    void reset_local_state();
    int parse_item(const uint8_t* data, size_t size, size_t* offset);
    int parse_main_item(const uint8_t* data, size_t size, size_t* offset, uint8_t tag, uint32_t value);
    int parse_global_item(const uint8_t* data, size_t size, size_t* offset, uint8_t tag, uint32_t value);
    int parse_local_item(const uint8_t* data, size_t size, size_t* offset, uint8_t tag, uint32_t value);
    void add_field(uint8_t report_type, uint32_t flags);
    
public:
    HIDReportDescriptor();
    ~HIDReportDescriptor();
    
    /* Parse HID report descriptor */
    int parse(const uint8_t* data, size_t size);
    
    /* Find field by usage */
    HIDReportField* find_field(const HIDUsage& usage, uint8_t report_type);
    
    /* Get all fields of a type */
    HIDReportField* get_fields(uint8_t report_type, uint32_t* count);
    
    /* Get root collection */
    HIDCollection* get_root_collection() { return root_collection; }
    
    /* Get report size for a type */
    size_t get_report_size(uint8_t report_type, uint8_t report_id);
};

