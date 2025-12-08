#ifndef ASTRAOS_DRIVERS_FRAMEBUFFER_H
#define ASTRAOS_DRIVERS_FRAMEBUFFER_H

#include <stdint.h>

void framebuffer_init(uint64_t addr, uint32_t width, uint32_t height, uint32_t pitch, uint32_t bpp);
void framebuffer_clear(uint32_t color);
uint32_t framebuffer_width(void);
uint32_t framebuffer_height(void);
/* Backward-compatible aliases */
uint32_t fb_get_width(void);
uint32_t fb_get_height(void);

#endif /* ASTRAOS_DRIVERS_FRAMEBUFFER_H */

