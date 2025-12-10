// USB HID keyboard driver.
#pragma once

#include <stdint.h>
#include <stddef.h>

#include "usb_core.hpp"

namespace usb {

class XhciController;

class UsbKeyboardDriver {
public:
    UsbKeyboardDriver();

    // Bind to a configured device and schedule first interrupt transfer.
    void bind(XhciController* ctrl, UsbDevice* dev);

    // Handle an 8-byte HID report.
    void handle_report(const uint8_t report[8]);

    // Map HID usage to internal keycode (returns 0 for unknown).
    uint8_t usage_to_key(uint8_t usage) const;

private:
    XhciController* ctrl_{nullptr};
    UsbDevice* dev_{nullptr};
    uint8_t interrupt_ep_{0};
    uint8_t last_report_[8]{};

    void dispatch_changes(const uint8_t report[8]);
    void submit_next();
};

}  // namespace usb
