#include "types.h"
#include "drivers.h"
#include "vfs.h"
#include "kmalloc.h"
#include "kernel.h"
#include "string.h"

/* Forward declaration */
extern uint32_t fb_get_bpp(void);

/* Custom allocators for lodepng to use kernel memory */
#define LODEPNG_NO_COMPILE_ALLOCATORS
#define LODEPNG_NO_COMPILE_CPP  /* Disable C++ wrapper */

#ifdef __cplusplus
extern "C" {
#endif

void* lodepng_malloc(size_t size) {
    return kmalloc(size);
}
void* lodepng_realloc(void* ptr, size_t new_size) {
    return krealloc(ptr, new_size);
}
void lodepng_free(void* ptr) {
    kfree(ptr);
}

#ifdef __cplusplus
}
#endif

#define LODEPNG_COMPILE_DECODER
#define LODEPNG_COMPILE_PNG

/* Use C linkage wrappers for lodepng functions */
extern unsigned lodepng_decode32_c(unsigned char** out, unsigned* w, unsigned* h,
                                    const unsigned char* in, size_t insize);
extern const char* lodepng_error_text_c(unsigned code);

/* Cursor image data */
static unsigned char *cursor_image = NULL;
static unsigned cursor_width = 0;
static unsigned cursor_height = 0;
static bool cursor_loaded = false;

/**
 * Load cursor image from file via VFS
 * @param path: Path to PNG file (e.g., "/assets/cursor.png" or "/usr/share/cursor.png")
 * @return 0 on success, non-zero on error
 */
int mouse_cursor_load_from_file(const char *path) {
    if (!path) return -1;
    
    vfs_node_t *node = vfs_lookup(NULL, path);
    if (!node || node->type != VFS_NODE_FILE) {
        printf("mouse_cursor: file not found: %s\n", path);
        return -1;
    }
    
    size_t file_size = node->size;
    if (file_size == 0) {
        printf("mouse_cursor: file is empty: %s\n", path);
        return -1;
    }
    
    /* Allocate buffer for file data */
    unsigned char *png_data = (unsigned char *)kmalloc(file_size);
    if (!png_data) {
        printf("mouse_cursor: failed to allocate %zu bytes for file\n", file_size);
        return -1;
    }
    
    /* Read entire file */
    ssize_t read_bytes = vfs_read(node, 0, file_size, png_data);
    if (read_bytes < 0 || (size_t)read_bytes != file_size) {
        printf("mouse_cursor: failed to read file: %s (read %zd/%zu)\n", path, read_bytes, file_size);
        kfree(png_data);
        return -1;
    }
    
    printf("mouse_cursor: loaded %zu bytes from %s\n", file_size, path);
    
    /* Decode PNG */
    int result = mouse_cursor_load_from_memory(png_data, file_size);
    
    /* Free file buffer */
    kfree(png_data);
    
    return result;
}

/**
 * Load cursor image from memory buffer (PNG data)
 * @param png_data: Pointer to PNG file data
 * @param png_size: Size of PNG data in bytes
 * @return 0 on success, non-zero on error
 */
int mouse_cursor_load_from_memory(const unsigned char *png_data, size_t png_size) {
    if (cursor_loaded && cursor_image) {
        lodepng_free(cursor_image);
        cursor_image = NULL;
        cursor_loaded = false;
    }

    printf("mouse_cursor: decoding PNG, size=%zu\n", png_size);
    printf("mouse_cursor: png_data=%p\n", (void *)png_data);
    
    /* Check if PNG data is valid */
    if (!png_data || png_size < 8) {
        printf("mouse_cursor: invalid PNG data\n");
        return -1;
    }
    
    /* Check PNG signature */
    if (png_data[0] != 0x89 || png_data[1] != 0x50 || png_data[2] != 0x4E || png_data[3] != 0x47) {
        printf("mouse_cursor: invalid PNG signature\n");
        return -1;
    }
    
    unsigned error = lodepng_decode32_c(&cursor_image, &cursor_width, &cursor_height, png_data, png_size);
    if (error) {
        const char *error_text = lodepng_error_text_c(error);
        printf("mouse_cursor: failed to decode PNG: error=%u", error);
        if (error_text) {
            printf(" - %s", error_text);
        }
        printf("\n");
        return -1;
    }

    cursor_loaded = true;
    printf("mouse_cursor: loaded cursor image %ux%u\n", cursor_width, cursor_height);
    return 0;
}

/**
 * Get cursor dimensions
 */
void mouse_cursor_get_size(unsigned *w, unsigned *h) {
    if (w) *w = cursor_width;
    if (h) *h = cursor_height;
}

/**
 * Draw cursor at position (x, y) on framebuffer
 * Supports alpha blending (transparency)
 * Note: Background should be saved before calling this function
 */
void mouse_cursor_draw(int x, int y) {
    if (!cursor_loaded || !cursor_image) return;

    uint32_t fb_w = fb_width();
    uint32_t fb_h = fb_height();
    uint32_t bpp = fb_get_bpp();

    for (unsigned py = 0; py < cursor_height; py++) {
        for (unsigned px = 0; px < cursor_width; px++) {
            int screen_x = x + (int)px;
            int screen_y = y + (int)py;

            if (screen_x < 0 || screen_y < 0 || screen_x >= (int)fb_w || screen_y >= (int)fb_h) {
                continue;
            }

            /* Get RGBA pixel from cursor image */
            unsigned idx = (py * cursor_width + px) * 4;
            uint8_t r = cursor_image[idx + 0];
            uint8_t g = cursor_image[idx + 1];
            uint8_t b = cursor_image[idx + 2];
            uint8_t a = cursor_image[idx + 3];

            /* Alpha blending: if fully transparent, skip */
            if (a == 0) continue;

            uint32_t color;

            /* Get background pixel for alpha blending */
            uint32_t bg = fb_getpixel((uint32_t)screen_x, (uint32_t)screen_y);
            
            /* Extract background color based on BPP */
            uint8_t bg_r, bg_g, bg_b;
            if (bpp == 32) {
                /* 32-bit: ARGB or RGBA format - check actual format */
                /* Most framebuffers use ARGB (0xAARRGGBB) or XRGB (0x00RRGGBB) */
                bg_r = (bg >> 16) & 0xFF;
                bg_g = (bg >> 8) & 0xFF;
                bg_b = bg & 0xFF;
            } else if (bpp == 24) {
                /* 24-bit: RGB */
                bg_r = (bg >> 16) & 0xFF;
                bg_g = (bg >> 8) & 0xFF;
                bg_b = bg & 0xFF;
            } else {
                /* Fallback: assume RGB */
                bg_r = (bg >> 16) & 0xFF;
                bg_g = (bg >> 8) & 0xFF;
                bg_b = bg & 0xFF;
            }

            /* Alpha blending: blend foreground with background */
            if (a < 255) {
                /* Blend: result = (alpha * foreground + (255-alpha) * background) / 255 */
                uint16_t alpha = a;
                uint16_t inv_alpha = 255 - alpha;
                uint8_t blend_r = ((uint16_t)r * alpha + (uint16_t)bg_r * inv_alpha) / 255;
                uint8_t blend_g = ((uint16_t)g * alpha + (uint16_t)bg_g * inv_alpha) / 255;
                uint8_t blend_b = ((uint16_t)b * alpha + (uint16_t)bg_b * inv_alpha) / 255;
                
                /* Format color for framebuffer (0xFFRRGGBB for 32-bit) */
                if (bpp == 32) {
                    color = 0xFF000000 | (blend_r << 16) | (blend_g << 8) | blend_b;
                } else {
                    color = (blend_r << 16) | (blend_g << 8) | blend_b;
                }
            } else {
                /* Fully opaque - use foreground color directly */
                if (bpp == 32) {
                    color = 0xFF000000 | (r << 16) | (g << 8) | b;
                } else {
                    color = (r << 16) | (g << 8) | b;
                }
            }

            fb_putpixel((uint32_t)screen_x, (uint32_t)screen_y, color);
        }
    }
}

/**
 * Cleanup cursor image
 */
void mouse_cursor_cleanup(void) {
    if (cursor_image) {
        lodepng_free(cursor_image);
        cursor_image = NULL;
        cursor_loaded = false;
    }
}

