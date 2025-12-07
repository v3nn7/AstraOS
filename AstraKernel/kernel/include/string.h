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

#ifdef __cplusplus
}
#endif
