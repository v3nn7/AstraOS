#include <klog.h>
#include <stdarg.h>

void klog_printf(int level, const char *fmt, ...) {
    (void)level;
    (void)fmt;
    /* Silent stub logger for freestanding build. */
    va_list args;
    va_start(args, fmt);
    va_end(args);
}

