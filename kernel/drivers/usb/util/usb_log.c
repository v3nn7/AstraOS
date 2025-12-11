/**
 * USB logging wrappers.
 */

#include "klog.h"
#include <stdarg.h>

static void vprint(int level, const char *fmt, va_list args) {
    klog_vprintf(level, fmt, args);
}

void usb_log(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vprint(KLOG_INFO, fmt, args);
    va_end(args);
}

void usb_warn(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vprint(KLOG_WARN, fmt, args);
    va_end(args);
}

void usb_error(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vprint(KLOG_ERROR, fmt, args);
    va_end(args);
}
