#pragma once

#include "types.h"

#ifdef __cplusplus
extern "C" {
#endif

void *k_memset(void *dest, int value, unsigned long n);
int strcmp(const char *a, const char *b);
size_t strlen(const char *s);
void *memcpy(void *dst, const void *src, unsigned long n);
char *strcpy(char *dst, const char *src);
int strncmp(const char *a, const char *b, size_t n);
int memcmp(const void *a, const void *b, size_t n);
char *strchr(const char *s, int c);
char *strrchr(const char *s, int c);

/* Provide a libc-like memset wrapper that delegates to k_memset. */
static inline void *memset(void *dest, int value, unsigned long n) {
    return k_memset(dest, value, n);
}

#ifdef __cplusplus
}
#endif
