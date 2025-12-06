#include "string.h"

void *memset(void *dest, int value, unsigned long n) {
    unsigned char *p = (unsigned char *)dest;
    while (n--) {
        *p++ = (unsigned char)value;
    }
    return dest;
}

size_t strlen(const char *s) {
    size_t n = 0;
    while (s && *s++) ++n;
    return n;
}

int strcmp(const char *a, const char *b) {
    while (*a && (*a == *b)) { a++; b++; }
    return (uint8_t)*a - (uint8_t)*b;
}
