#include <lib/printf.h>
#include <stdarg.h>
#include <stdio.h>

void kprintf(const char *fmt, ...) {
    (void)fmt;

    va_list args;
    va_start(args, fmt);
    va_end(args);
    /* Stub: output formatted text to console */
}

int printf(const char *fmt, ...) {
    (void)fmt;
    va_list args;
    va_start(args, fmt);
    va_end(args);
    return 0;
}

