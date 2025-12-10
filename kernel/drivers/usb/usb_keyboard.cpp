// USB HID keyboard driver implementation.
#include "usb_keyboard.hpp"

#include <stddef.h>

#include "xhci.hpp"

namespace usb {

namespace {

// Minimal HID usage to keycode mapping for 0x04-0x39.
uint8_t usage_table(uint8_t usage) {
    switch (usage) {
        case 0x04: return 'a';
        case 0x05: return 'b';
        case 0x06: return 'c';
        case 0x07: return 'd';
        case 0x08: return 'e';
        case 0x09: return 'f';
        case 0x0A: return 'g';
        case 0x0B: return 'h';
        case 0x0C: return 'i';
        case 0x0D: return 'j';
        case 0x0E: return 'k';
        case 0x0F: return 'l';
        case 0x10: return 'm';
        case 0x11: return 'n';
        case 0x12: return 'o';
        case 0x13: return 'p';
        case 0x14: return 'q';
        case 0x15: return 'r';
        case 0x16: return 's';
        case 0x17: return 't';
        case 0x18: return 'u';
        case 0x19: return 'v';
        case 0x1A: return 'w';
        case 0x1B: return 'x';
        case 0x1C: return 'y';
        case 0x1D: return 'z';
        case 0x1E: return '1';
        case 0x1F: return '2';
        case 0x20: return '3';
        case 0x21: return '4';
        case 0x22: return '5';
        case 0x23: return '6';
        case 0x24: return '7';
        case 0x25: return '8';
        case 0x26: return '9';
        case 0x27: return '0';
        case 0x28: return '\n';
        case 0x29: return 0x1B;  // ESC
        case 0x2A: return '\b';
        case 0x2B: return '\t';
        case 0x2C: return ' ';
        case 0x2D: return '-';
        case 0x2E: return '=';
        case 0x2F: return '[';
        case 0x30: return ']';
        case 0x31: return '\\';
        case 0x33: return ';';
        case 0x34: return '\'';
        case 0x35: return '`';
        case 0x36: return ',';
        case 0x37: return '.';
        case 0x38: return '/';
        case 0x39: return 0x39;  // CapsLock symbolic
        default: return 0;
    }
}

}  // namespace

UsbKeyboardDriver::UsbKeyboardDriver() = default;

void UsbKeyboardDriver::bind(XhciController* ctrl, UsbDevice* dev) {
    ctrl_ = ctrl;
    dev_ = dev;
    // Find first interrupt IN endpoint (bit7 set for IN).
    interrupt_ep_ = 0;
    if (dev && dev->configuration() && dev->configuration()->interface_count > 0) {
        UsbInterface& ifc = dev->configuration()->interfaces[0];
        for (uint8_t i = 0; i < ifc.endpoint_count; ++i) {
            UsbEndpoint& ep = ifc.endpoints[i];
            if (ep.type == UsbTransferType::Interrupt && (ep.address & 0x80)) {
                interrupt_ep_ = ep.address;
                break;
            }
        }
    }
    submit_next();
}

uint8_t UsbKeyboardDriver::usage_to_key(uint8_t usage) const {
    return usage_table(usage);
}

void UsbKeyboardDriver::dispatch_changes(const uint8_t report[8]) {
    // Compare new report vs previous to emit key events.
    for (size_t i = 2; i < 8; ++i) {
        const uint8_t usage = report[i];
        bool present_before = false;
        for (size_t j = 2; j < 8; ++j) {
            if (last_report_[j] == usage && usage != 0) {
                present_before = true;
            }
        }
        if (usage != 0 && !present_before) {
            uint8_t key = usage_to_key(usage);
            if (key) {
                input_push_key(key, true);
            }
        }
    }
    for (size_t i = 2; i < 8; ++i) {
        const uint8_t usage = last_report_[i];
        bool still_present = false;
        for (size_t j = 2; j < 8; ++j) {
            if (report[j] == usage && usage != 0) {
                still_present = true;
            }
        }
        if (usage != 0 && !still_present) {
            uint8_t key = usage_to_key(usage);
            if (key) {
                input_push_key(key, false);
            }
        }
    }
}

void UsbKeyboardDriver::handle_report(const uint8_t report[8]) {
    dispatch_changes(report);
    for (size_t i = 0; i < 8; ++i) {
        last_report_[i] = report[i];
    }
    submit_next();
}

void UsbKeyboardDriver::submit_next() {
    static uint8_t buffer[8];
    if (ctrl_ && dev_ && interrupt_ep_ != 0) {
        ctrl_->submit_interrupt_in(dev_, interrupt_ep_, buffer, sizeof(buffer));
    }
}

}  // namespace usb
