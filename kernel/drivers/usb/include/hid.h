#ifndef USB_HID_H
#define USB_HID_H

#include <stdint.h>

/* Basic HID usage information used by keyboards and mice. */
typedef struct hid_report_info {
    uint16_t usage_page;
    uint16_t usage_id;
    uint8_t report_id;
} hid_report_info_t;

#endif /* USB_HID_H */
