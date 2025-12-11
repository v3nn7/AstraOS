#ifndef USB_HID_H
#define USB_HID_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/* Basic HID usage information used by keyboards and mice. */
typedef struct hid_report_info {
    uint16_t usage_page;
    uint16_t usage_id;
    uint8_t report_id;
} hid_report_info_t;

/* Parse a simple HID usage from raw bytes (boot protocol). */
static inline hid_report_info_t hid_make_usage(uint16_t page, uint16_t id, uint8_t rid) {
    hid_report_info_t info;
    info.usage_page = page;
    info.usage_id = id;
    info.report_id = rid;
    return info;
}

/* Validate a boot-protocol HID report blob (stub: non-empty check). */
bool hid_parser_validate_boot_report(const uint8_t* report, size_t len);

#endif /* USB_HID_H */
