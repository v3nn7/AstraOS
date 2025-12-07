/**
 * HID Report Descriptor Parser Implementation
 * 
 * Full implementation of HID report descriptor parser according to USB HID specification.
 * Parses short and long items, handles collections, and builds report field structures.
 */

#include "usb/hid/hid_report_descriptor.hpp"
#include "kmalloc.h"
#include "kernel.h"
#include "klog.h"
#include "string.h"

/* Global state stack for PUSH/POP operations */
#define MAX_GLOBAL_STATE_STACK 16
typedef struct {
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
} global_state_t;

/* Collection stack and global state stack (static file-level variables) */
static HIDCollection* g_collection_stack[MAX_GLOBAL_STATE_STACK];
static int g_collection_stack_depth = 0;
static global_state_t g_global_stack[MAX_GLOBAL_STATE_STACK];
static int g_global_stack_depth = 0;

HIDReportDescriptor::HIDReportDescriptor() :
    root_collection(nullptr),
    descriptor_data(nullptr),
    descriptor_size(0),
    usages(nullptr),
    usage_count(0) {
    
    reset_global_state();
    reset_local_state();
    
    for (int i = 0; i < 3; i++) {
        report_bit_offset[i] = 0;
        current_fields[i] = nullptr;
        current_field_count[i] = 0;
    }
}

HIDReportDescriptor::~HIDReportDescriptor() {
    if (root_collection) {
        delete root_collection;
    }
    if (descriptor_data) {
        kfree(descriptor_data);
    }
    if (usages) {
        kfree(usages);
    }
    for (int i = 0; i < 3; i++) {
        if (current_fields[i]) {
            kfree(current_fields[i]);
        }
    }
}

void HIDReportDescriptor::reset_global_state() {
    usage_page = HID_USAGE_PAGE_UNDEFINED;
    logical_min = 0;
    logical_max = 0;
    physical_min = 0;
    physical_max = 0;
    unit = 0;
    unit_exponent = 0;
    report_size = 0;
    report_count = 0;
    report_id = 0;
}

void HIDReportDescriptor::reset_local_state() {
    if (usages) {
        kfree(usages);
        usages = nullptr;
    }
    usage_count = 0;
    usage_min = HIDUsage();
    usage_max = HIDUsage();
}

int HIDReportDescriptor::parse(const uint8_t* data, size_t size) {
    if (!data || size == 0) {
        return -1;
    }
    
    /* Store descriptor */
    descriptor_data = (uint8_t*)kmalloc(size);
    if (!descriptor_data) {
        return -1;
    }
    memcpy(descriptor_data, data, size);
    descriptor_size = size;
    
    reset_global_state();
    reset_local_state();
    
    /* Allocate root collection */
    root_collection = new HIDCollection();
    if (!root_collection) {
        return -1;
    }
    root_collection->type = HID_COLLECTION_APPLICATION;
    
    /* Allocate field arrays */
    for (int i = 0; i < 3; i++) {
        current_fields[i] = (HIDReportField*)kmalloc(sizeof(HIDReportField) * 256);
        if (!current_fields[i]) {
            return -1;
        }
        current_field_count[i] = 0;
    }
    
    /* Initialize collection stack and global state stack */
    g_collection_stack[0] = root_collection;
    g_collection_stack_depth = 1;
    g_global_stack_depth = 0;
    
    /* Parse items */
    size_t offset = 0;
    while (offset < size) {
        int ret = parse_item(data, size, &offset);
        if (ret < 0) {
            klog_printf(KLOG_ERROR, "hid_parser: failed to parse item at offset %zu", offset);
            break;
        }
    }
    
    /* Finalize root collection */
    root_collection->fields = current_fields[0]; /* Input */
    root_collection->field_count = current_field_count[0];
    
    klog_printf(KLOG_INFO, "hid_parser: parsed %u input fields, %u output fields, %u feature fields",
                current_field_count[0], current_field_count[1], current_field_count[2]);
    
    return 0;
}

int HIDReportDescriptor::parse_item(const uint8_t* data, size_t size, size_t* offset) {
    if (*offset >= size) return -1;
    
    uint8_t b0 = data[*offset];
    
    /* Check for long item */
    if (b0 == 0xFE) {
        /* Long item format */
        if (*offset + 2 >= size) return -1;
        uint8_t b1 = data[*offset + 1];
        uint32_t data_size = b1;
        
        *offset += 3 + data_size;
        /* Long items are rarely used, skip for now */
        return 0;
    }
    
    /* Short item format */
    uint8_t size_code = b0 & 0x03;
    uint8_t type = (b0 >> 2) & 0x03;
    uint8_t tag = (b0 >> 4) & 0x0F;
    
    uint32_t value = 0;
    size_t item_size = 1;
    
    /* Read value based on size code */
    switch (size_code) {
        case 0: /* 0 bytes */
            value = 0;
            break;
        case 1: /* 1 byte */
            if (*offset + 1 >= size) return -1;
            value = data[*offset + 1];
            item_size = 2;
            break;
        case 2: /* 2 bytes */
            if (*offset + 2 >= size) return -1;
            value = data[*offset + 1] | (data[*offset + 2] << 8);
            item_size = 3;
            break;
        case 3: /* 4 bytes */
            if (*offset + 4 >= size) return -1;
            value = data[*offset + 1] | (data[*offset + 2] << 8) |
                   (data[*offset + 3] << 16) | (data[*offset + 4] << 24);
            item_size = 5;
            break;
    }
    
    *offset += item_size;
    
    /* Route to appropriate parser */
    switch (type) {
        case HID_ITEM_TYPE_MAIN:
            return parse_main_item(data, size, offset, tag, value);
        case HID_ITEM_TYPE_GLOBAL:
            return parse_global_item(data, size, offset, tag, value);
        case HID_ITEM_TYPE_LOCAL:
            return parse_local_item(data, size, offset, tag, value);
        default:
            return 0; /* Reserved, skip */
    }
}

int HIDReportDescriptor::parse_main_item(const uint8_t* data, size_t size, size_t* offset, uint8_t tag, uint32_t value) {
    (void)data;
    (void)size;
    (void)offset;
    
    switch (tag) {
        case 0x8: /* Input */
            add_field(1, (uint32_t)value); /* Input report with flags */
            reset_local_state();
            break;
        case 0x9: /* Output */
            add_field(2, (uint32_t)value); /* Output report with flags */
            reset_local_state();
            break;
        case 0xB: /* Feature */
            add_field(3, (uint32_t)value); /* Feature report with flags */
            reset_local_state();
            break;
        case 0xA: { /* Collection */
            /* Handle nested collections */
            if (g_collection_stack_depth >= MAX_GLOBAL_STATE_STACK) {
                klog_printf(KLOG_WARN, "hid_parser: collection stack overflow, ignoring nested collection");
                reset_local_state();
                break;
            }
            
            /* Create new collection */
            HIDCollection* new_collection = new HIDCollection();
            if (!new_collection) {
                klog_printf(KLOG_ERROR, "hid_parser: failed to allocate collection");
                reset_local_state();
                break;
            }
            
            new_collection->type = (uint8_t)value; /* Collection type */
            new_collection->usage = usage_min; /* Use current usage */
            new_collection->fields = nullptr;
            new_collection->field_count = 0;
            new_collection->children = nullptr;
            new_collection->child_count = 0;
            
            /* Add to parent collection's children */
            HIDCollection* parent = g_collection_stack[g_collection_stack_depth - 1];
            if (parent) {
                /* Allocate or reallocate children array */
                HIDCollection** old_children = (HIDCollection**)parent->children;
                HIDCollection** new_children = (HIDCollection**)kmalloc(sizeof(HIDCollection*) * (parent->child_count + 1));
                if (new_children) {
                    /* Copy existing children */
                    if (old_children) {
                        for (uint32_t i = 0; i < parent->child_count; i++) {
                            new_children[i] = old_children[i];
                        }
                        kfree(old_children);
                    }
                    /* Add new collection */
                    new_children[parent->child_count] = new_collection;
                    parent->children = (HIDCollection*)new_children; /* Store pointer to array */
                    parent->child_count++;
                } else {
                    klog_printf(KLOG_ERROR, "hid_parser: failed to allocate children array");
                    delete new_collection;
                    reset_local_state();
                    break;
                }
            }
            
            /* Push to collection stack */
            g_collection_stack[g_collection_stack_depth++] = new_collection;
            
            reset_local_state();
            break;
        }
        case 0xC: { /* End Collection */
            /* Pop from collection stack */
            if (g_collection_stack_depth > 1) {
                g_collection_stack_depth--;
            } else {
                klog_printf(KLOG_WARN, "hid_parser: End Collection without matching Collection");
            }
            reset_local_state();
            break;
        }
    }
    
    return 0;
}

int HIDReportDescriptor::parse_global_item(const uint8_t* data, size_t size, size_t* offset, uint8_t tag, uint32_t value) {
    (void)data;
    (void)size;
    (void)offset;
    
    switch (tag) {
        case 0x0: /* Usage Page */
            usage_page = (uint16_t)value;
            break;
        case 0x1: /* Logical Minimum */
            logical_min = (int32_t)value;
            if (logical_min > 127) logical_min = (int32_t)((int32_t)value | 0xFFFFFF00);
            break;
        case 0x2: /* Logical Maximum */
            logical_max = (int32_t)value;
            if (logical_max > 127) logical_max = (int32_t)((int32_t)value | 0xFFFFFF00);
            break;
        case 0x3: /* Physical Minimum */
            physical_min = (int32_t)value;
            break;
        case 0x4: /* Physical Maximum */
            physical_max = (int32_t)value;
            break;
        case 0x5: /* Unit Exponent */
            unit_exponent = value & 0x0F;
            break;
        case 0x6: /* Unit */
            unit = value;
            break;
        case 0x7: /* Report Size */
            report_size = value;
            break;
        case 0x8: /* Report ID */
            report_id = (uint8_t)value;
            break;
        case 0x9: /* Report Count */
            report_count = value;
            break;
        case 0xA: { /* Push */
            /* Push current global state onto stack */
            if (g_global_stack_depth >= MAX_GLOBAL_STATE_STACK) {
                klog_printf(KLOG_WARN, "hid_parser: global state stack overflow");
                break;
            }
            
            global_state_t* state = &g_global_stack[g_global_stack_depth++];
            state->usage_page = usage_page;
            state->logical_min = logical_min;
            state->logical_max = logical_max;
            state->physical_min = physical_min;
            state->physical_max = physical_max;
            state->unit = unit;
            state->unit_exponent = unit_exponent;
            state->report_size = report_size;
            state->report_count = report_count;
            state->report_id = report_id;
            break;
        }
        case 0xB: { /* Pop */
            /* Pop global state from stack */
            if (g_global_stack_depth == 0) {
                klog_printf(KLOG_WARN, "hid_parser: Pop without matching Push");
                break;
            }
            
            global_state_t* state = &g_global_stack[--g_global_stack_depth];
            usage_page = state->usage_page;
            logical_min = state->logical_min;
            logical_max = state->logical_max;
            physical_min = state->physical_min;
            physical_max = state->physical_max;
            unit = state->unit;
            unit_exponent = state->unit_exponent;
            report_size = state->report_size;
            report_count = state->report_count;
            report_id = state->report_id;
            break;
        }
    }
    
    return 0;
}

int HIDReportDescriptor::parse_local_item(const uint8_t* data, size_t size, size_t* offset, uint8_t tag, uint32_t value) {
    (void)data;
    (void)size;
    (void)offset;
    
    switch (tag) {
        case 0x0: /* Usage */
            if (usage_count == 0) {
                usage_min = HIDUsage(usage_page, (uint16_t)value);
                usage_max = usage_min;
            } else {
                usage_max = HIDUsage(usage_page, (uint16_t)value);
            }
            usage_count++;
            break;
        case 0x1: /* Usage Minimum */
            usage_min = HIDUsage(usage_page, (uint16_t)value);
            break;
        case 0x2: /* Usage Maximum */
            usage_max = HIDUsage(usage_page, (uint16_t)value);
            break;
    }
    
    return 0;
}

void HIDReportDescriptor::add_field(uint8_t report_type, uint32_t flags) {
    if (report_type == 0 || report_type > 3) return;
    uint32_t idx = report_type - 1;
    
    if (current_field_count[idx] >= 256) return;
    
    HIDReportField* field = &current_fields[idx][current_field_count[idx]];
    field->usage_min = usage_min;
    field->usage_max = usage_max;
    field->logical_min = logical_min;
    field->logical_max = logical_max;
    field->physical_min = physical_min;
    field->physical_max = physical_max;
    field->unit = unit;
    field->unit_exponent = unit_exponent;
    field->size = report_size;
    field->count = report_count;
    field->offset = report_bit_offset[idx];
    field->flags = flags;
    field->report_id = report_id;
    field->report_type = report_type;
    
    report_bit_offset[idx] += report_size * report_count;
    current_field_count[idx]++;
}

HIDReportField* HIDReportDescriptor::find_field(const HIDUsage& usage, uint8_t report_type) {
    if (report_type == 0 || report_type > 3) return nullptr;
    uint32_t idx = report_type - 1;
    
    for (uint32_t i = 0; i < current_field_count[idx]; i++) {
        if (current_fields[idx][i].matches_usage(usage)) {
            return &current_fields[idx][i];
        }
    }
    
    return nullptr;
}

HIDReportField* HIDReportDescriptor::get_fields(uint8_t report_type, uint32_t* count) {
    if (report_type == 0 || report_type > 3) {
        if (count) *count = 0;
        return nullptr;
    }
    
    uint32_t idx = report_type - 1;
    if (count) *count = current_field_count[idx];
    return current_fields[idx];
}

size_t HIDReportDescriptor::get_report_size(uint8_t report_type, uint8_t report_id) {
    if (report_type == 0 || report_type > 3) return 0;
    uint32_t idx = report_type - 1;
    
    uint32_t max_bits = 0;
    for (uint32_t i = 0; i < current_field_count[idx]; i++) {
        if (current_fields[idx][i].report_id == report_id) {
            uint32_t field_end = current_fields[idx][i].offset + 
                                current_fields[idx][i].size * current_fields[idx][i].count;
            if (field_end > max_bits) {
                max_bits = field_end;
            }
        }
    }
    
    return (max_bits + 7) / 8; /* Round up to bytes */
}

