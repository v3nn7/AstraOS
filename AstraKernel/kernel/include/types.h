#pragma once

typedef unsigned char  uint8_t;
typedef unsigned short uint16_t;
typedef unsigned int   uint32_t;
typedef unsigned long  uint64_t;

typedef signed char    int8_t;
typedef signed short   int16_t;
typedef signed int     int32_t;
typedef signed long    int64_t;

typedef unsigned long  size_t;
typedef signed long    ssize_t;
typedef uint64_t       phys_addr_t;
typedef uint64_t       virt_addr_t;

typedef enum { false = 0, true = 1 } bool;

#define PACKED __attribute__((packed))
#define ALIGNED(x) __attribute__((aligned(x)))

