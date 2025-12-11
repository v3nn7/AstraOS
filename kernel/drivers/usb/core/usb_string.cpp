/**
 * C++ helpers for USB string descriptors.
 */

#include "usb_string.hpp"

namespace {
__attribute__((unused)) static void usb_string_selftest() {
    const uint8_t demo[] = {6, 3, 'T', 0, 'E', 0};
    auto s = usb_string_from_descriptor(demo, sizeof(demo));
    (void)s;
}
}
