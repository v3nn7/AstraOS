#include "string.h"

/* Use D implementation for performance if available */
#ifdef HAVE_D_MEMSET
extern void *k_memset(void *dest, int value, unsigned long n);
void *memset(void *dest, int value, unsigned long n) {
    return k_memset(dest, value, n);
}
#else
void *memset(void *dest, int value, unsigned long n) {
    unsigned char *p = (unsigned char *)dest;
    while (n--) *p++ = (unsigned char)value;
    return dest;
}
#endif

size_t strlen(const char *s) {
    size_t n = 0;
    while (s && *s++) ++n;
    return n;
}

int strcmp(const char *a, const char *b) {
    while (*a && (*a == *b)) { a++; b++; }
    return (uint8_t)*a - (uint8_t)*b;
}

int strncmp(const char *a, const char *b, size_t n) {
    while (n && *a && (*a == *b)) {
        a++; b++; n--;
    }
    if (n == 0) return 0;
    return (uint8_t)*a - (uint8_t)*b;
}

char *strcpy(char *dst, const char *src) {
    char *d = dst;
    while ((*d++ = *src++)) {}
    return dst;
}

void *memcpy(void *dst, const void *src, unsigned long n) {
    unsigned char *d = dst;
    const unsigned char *s = src;
    while (n--) *d++ = *s++;
    return dst;
}
