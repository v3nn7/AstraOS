#include "limine.h"

__attribute__((used, section(".limine_reqs")))
volatile struct limine_framebuffer_request limine_fb_request = {
    .id = LIMINE_FRAMEBUFFER_REQUEST,
    .revision = 0,
    .response = 0,
};

__attribute__((used, section(".limine_reqs")))
volatile struct limine_memmap_request limine_memmap_request = {
    .id = LIMINE_MEMMAP_REQUEST,
    .revision = 0,
    .response = 0,
};
