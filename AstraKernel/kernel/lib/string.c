#include "string.h"

void *memset(void *dest, int value, unsigned long n) {
    return k_memset(dest, value, n);
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
