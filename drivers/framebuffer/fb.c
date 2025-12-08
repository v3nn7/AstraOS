#include <drivers/framebuffer.h>
#include <stdint.h>

static volatile uint8_t *fb = 0;
static uint32_t fb_w = 0, fb_h = 0, fb_pitch = 0, fb_bpp = 0;

void framebuffer_init(uint64_t addr, uint32_t width, uint32_t height, uint32_t pitch, uint32_t bpp) {
    fb = (volatile uint8_t *)(uintptr_t)addr;
    fb_w = width;
    fb_h = height;
    fb_pitch = pitch;
    fb_bpp = bpp;
}

void framebuffer_clear(uint32_t color) {
    if (!fb || fb_bpp != 32) return;
    for (uint32_t y = 0; y < fb_h; y++) {
        uint32_t *row = (uint32_t *)(fb + y * fb_pitch);
        for (uint32_t x = 0; x < fb_w; x++) {
            row[x] = color;
        }
    }
}

uint32_t framebuffer_width(void) { return fb_w; }
uint32_t framebuffer_height(void) { return fb_h; }

uint32_t fb_get_width(void) { return framebuffer_width(); }
uint32_t fb_get_height(void) { return framebuffer_height(); }

