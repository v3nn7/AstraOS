#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

static inline std::string usb_string_from_descriptor(const uint8_t *raw, std::size_t len) {
    if (!raw || len < 2) {
        return {};
    }
    /* Skip first byte (length) and second (descriptor type) if present. */
    std::size_t start = 2;
    if (len <= start) {
        return {};
    }
    std::string out;
    out.reserve((len - start) / 2);
    for (std::size_t i = start; i + 1 < len; i += 2) {
        char c = static_cast<char>(raw[i]);
        out.push_back(c);
    }
    return out;
}
