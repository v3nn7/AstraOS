#include <string.h>

size_t kstrlen(const char *str) {
    size_t len = 0;
    if (!str) {
        return 0;
    }

    while (str[len] != '\0') {
        len++;
    }
    return len;
}

void *k_memset(void *dst, int value, size_t len) {
    unsigned char *p = (unsigned char *)dst;
    unsigned char v = (unsigned char)value;
    for (size_t i = 0; i < len; i++) {
        p[i] = v;
    }
    return dst;
}

void *k_memcpy(void *dst, const void *src, size_t len) {
    unsigned char *d = (unsigned char *)dst;
    const unsigned char *s = (const unsigned char *)src;
    for (size_t i = 0; i < len; i++) {
        d[i] = s[i];
    }
    return dst;
}

