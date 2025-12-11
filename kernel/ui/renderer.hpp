#pragma once
#include <stdint.h>
#include <stddef.h>

#include "../efi/gop.hpp"

struct Rgb {
    uint8_t r;
    uint8_t g;
    uint8_t b;
};

void renderer_init(EFI_GRAPHICS_OUTPUT_PROTOCOL* gop);
void renderer_clear(Rgb color);
void renderer_rect(uint32_t x, uint32_t y, uint32_t w, uint32_t h, Rgb color);
void renderer_rect_outline(uint32_t x, uint32_t y, uint32_t w, uint32_t h, Rgb color);
void renderer_text(const char* text, uint32_t x, uint32_t y, Rgb fg, Rgb bg);
uint32_t renderer_width();
uint32_t renderer_height();
