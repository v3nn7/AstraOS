#pragma once

#include "types.h"

/**
 * HID Usage Pages and Usages
 * 
 * Defines standard HID usage pages and usage codes according to USB HID specification.
 */

/* HID Usage Pages */
#define HID_USAGE_PAGE_UNDEFINED           0x00
#define HID_USAGE_PAGE_GENERIC_DESKTOP     0x01
#define HID_USAGE_PAGE_SIMULATION          0x02
#define HID_USAGE_PAGE_VR                  0x03
#define HID_USAGE_PAGE_SPORT               0x04
#define HID_USAGE_PAGE_GAME                0x05
#define HID_USAGE_PAGE_GENERIC_DEVICE      0x06
#define HID_USAGE_PAGE_KEYBOARD            0x07
#define HID_USAGE_PAGE_LED                 0x08
#define HID_USAGE_PAGE_BUTTON              0x09
#define HID_USAGE_PAGE_ORDINAL             0x0A
#define HID_USAGE_PAGE_TELEPHONY           0x0B
#define HID_USAGE_PAGE_CONSUMER            0x0C
#define HID_USAGE_PAGE_DIGITIZER           0x0D
#define HID_USAGE_PAGE_PID                 0x0F
#define HID_USAGE_PAGE_UNICODE             0x10
#define HID_USAGE_PAGE_ALPHANUMERIC        0x14

/* Generic Desktop Usage Codes */
#define HID_USAGE_GD_POINTER               0x01
#define HID_USAGE_GD_MOUSE                 0x02
#define HID_USAGE_GD_JOYSTICK              0x04
#define HID_USAGE_GD_GAMEPAD               0x05
#define HID_USAGE_GD_KEYBOARD              0x06
#define HID_USAGE_GD_KEYPAD                0x07
#define HID_USAGE_GD_X                     0x30
#define HID_USAGE_GD_Y                     0x31
#define HID_USAGE_GD_Z                     0x32
#define HID_USAGE_GD_RX                    0x33
#define HID_USAGE_GD_RY                    0x34
#define HID_USAGE_GD_RZ                    0x35
#define HID_USAGE_GD_SLIDER                0x36
#define HID_USAGE_GD_DIAL                  0x37
#define HID_USAGE_GD_WHEEL                 0x38
#define HID_USAGE_GD_HAT_SWITCH            0x39
#define HID_USAGE_GD_DPAD_UP               0x90
#define HID_USAGE_GD_DPAD_DOWN             0x91
#define HID_USAGE_GD_DPAD_RIGHT            0x92
#define HID_USAGE_GD_DPAD_LEFT             0x93

/* Button Usage Codes */
#define HID_USAGE_BUTTON_1                 0x01
#define HID_USAGE_BUTTON_2                 0x02
#define HID_USAGE_BUTTON_3                 0x03
#define HID_USAGE_BUTTON_4                 0x04
#define HID_USAGE_BUTTON_5                 0x05

/* Consumer Page Usage Codes */
#define HID_USAGE_CONSUMER_CONTROL          0x01
#define HID_USAGE_CONSUMER_AC_PAN          0x0238

/* HID Usage Structure */
struct HIDUsage {
    uint16_t page;
    uint16_t usage;
    
    HIDUsage() : page(0), usage(0) {}
    HIDUsage(uint16_t p, uint16_t u) : page(p), usage(u) {}
    
    bool operator==(const HIDUsage& other) const {
        return page == other.page && usage == other.usage;
    }
    
    bool operator<(const HIDUsage& other) const {
        if (page != other.page) return page < other.page;
        return usage < other.usage;
    }
};

