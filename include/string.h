#pragma once

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

size_t kstrlen(const char *str);
void *k_memset(void *dst, int value, size_t len);
void *k_memcpy(void *dst, const void *src, size_t len);

#ifdef __cplusplus
}
#endif

#define memcpy  k_memcpy
#define memset  k_memset

