#pragma once

#include "types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Load cursor image from PNG data in memory
 * @param png_data: Pointer to PNG file data
 * @param png_size: Size of PNG data in bytes
 * @return 0 on success, non-zero on error
 */
int mouse_cursor_load_from_memory(const unsigned char *png_data, size_t png_size);

/**
 * Get cursor dimensions
 * @param w: Output width (can be NULL)
 * @param h: Output height (can be NULL)
 */
void mouse_cursor_get_size(unsigned *w, unsigned *h);

/**
 * Draw cursor at position (x, y) on framebuffer
 * Supports alpha blending (transparency)
 * @param x: X coordinate (top-left corner)
 * @param y: Y coordinate (top-left corner)
 */
void mouse_cursor_draw(int x, int y);

/**
 * Cleanup cursor image and free memory
 */
void mouse_cursor_cleanup(void);

#ifdef __cplusplus
}
#endif

