#pragma once

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#define PACKED __attribute__((packed))
#define ALIGNED(x) __attribute__((aligned(x)))

typedef uint64_t phys_addr_t;

