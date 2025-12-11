#include "../include/hid.h"
#include <stddef.h>

bool hid_parser_validate_boot_report(const uint8_t* report, size_t len) {
    if (!report || len == 0) {
        return false;
    }
    /* Boot keyboard reports are typically 8 bytes; mouse 3-4 bytes. */
    if (len == 8) {
        return true;
    }
    if (len == 3 || len == 4) {
        return true;
    }
    return false;
}
