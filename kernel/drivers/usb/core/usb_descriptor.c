#include "../include/usb_descriptor.h"
#include <stddef.h>

bool usb_descriptor_is_valid(const usb_descriptor_header_t* hdr, size_t total_len) {
    if (!hdr || hdr->bLength == 0 || hdr->bLength > total_len) {
        return false;
    }
    return true;
}
