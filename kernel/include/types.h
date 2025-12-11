#pragma once

typedef unsigned char      uint8_t;
typedef unsigned short     uint16_t;
typedef unsigned int       uint32_t;
typedef unsigned long long uint64_t;

typedef signed char        int8_t;
typedef signed short       int16_t;
typedef signed int         int32_t;
typedef signed long long   int64_t;

typedef uint64_t           size_t;
typedef int64_t            ssize_t;

typedef uint64_t           phys_addr_t;
typedef uint64_t           virt_addr_t;
typedef uint64_t           uintptr_t;

#ifndef __cplusplus
typedef enum { false = 0, true = 1 } bool;
#else
typedef bool __cpp_bool_guard; /* no-op to avoid redefinition */
#endif

#ifndef NULL
#ifdef __cplusplus
#define NULL 0
#else
#define NULL ((void *)0)
#endif
#endif

#define PACKED      __attribute__((packed))
#define ALIGNED(x)  __attribute__((aligned(x)))
