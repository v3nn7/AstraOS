#pragma once

/*
 * AstraOS Source-Available License (ASAL v2.1)
 * Copyright (c) 2025 Krystian "v3nn7"
 * All rights reserved.
 *
 * Viewing allowed.
 * Modification, forking, redistribution, and commercial use prohibited
 * without explicit written permission from the author.
 *
 * Full license: see LICENSE.md or https://github.com/v3nn7
 */

/*
 * In host tests (HOST_TEST), include standard headers to avoid typedef
 * redefinitions with the system libc. For the kernel build, provide our own
 * fixed-width types and aliases.
 */
#ifdef HOST_TEST
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
typedef uintptr_t phys_addr_t;
typedef uintptr_t virt_addr_t;
#else

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
#include <stdbool.h>
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

#endif /* HOST_TEST */

#define PACKED      __attribute__((packed))
#define ALIGNED(x)  __attribute__((aligned(x)))
