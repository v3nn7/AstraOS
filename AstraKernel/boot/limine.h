#ifndef LIMINE_H
#define LIMINE_H 1

#include "types.h"

#define LIMINE_COMMON_MAGIC 0xC7B1DD30DF4C8B88ULL

struct limine_uuid {
    uint32_t a;
    uint16_t b;
    uint16_t c;
    uint8_t  d[8];
};

struct limine_common_response {
    uint64_t revision;
};

#define LIMINE_FRAMEBUFFER_REQUEST  { LIMINE_COMMON_MAGIC, 0x9d5827dcd881dd75ULL }
#define LIMINE_MEMMAP_REQUEST       { LIMINE_COMMON_MAGIC, 0x67cf3d1de78cceb7ULL }

struct limine_framebuffer_request {
    uint64_t id[2];
    uint64_t revision;
    void *response;
};

struct limine_framebuffer {
    void *address;
    uint16_t width;
    uint16_t height;
    uint16_t pitch;
    uint16_t bpp;
    uint8_t  memory_model;
    uint8_t  red_mask_size;
    uint8_t  red_mask_shift;
    uint8_t  green_mask_size;
    uint8_t  green_mask_shift;
    uint8_t  blue_mask_size;
    uint8_t  blue_mask_shift;
    uint8_t  reserved;
};

struct limine_framebuffer_response {
    struct limine_common_response common;
    uint64_t framebuffer_count;
    struct limine_framebuffer **framebuffers;
};

struct limine_memmap_entry {
    uint64_t base;
    uint64_t length;
    uint64_t type;
};

struct limine_memmap_response {
    struct limine_common_response common;
    uint64_t entry_count;
    struct limine_memmap_entry **entries;
};

struct limine_memmap_request {
    uint64_t id[2];
    uint64_t revision;
    void *response;
};

#endif
