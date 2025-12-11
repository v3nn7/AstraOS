#pragma once

#include "types.h"
#include "mmio.h"
#include "kmalloc.h"

#ifndef PAGE_SIZE
#define PAGE_SIZE 4096
#endif

#define PAGE_WRITE        (1u << 0)
#define PAGE_CACHE_DISABLE (1u << 1)
