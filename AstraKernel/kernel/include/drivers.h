#pragma once

#include "types.h"
#include "interrupts.h"

void keyboard_init(void);
bool keyboard_available(void);
bool keyboard_read_char(char *ch_out);
bool keyboard_poll_char(char *ch_out); /* polling fallback */

void fb_init(uint64_t addr, uint32_t width, uint32_t height, uint32_t pitch, uint32_t bpp);
uint32_t fb_width(void);
uint32_t fb_height(void);
uint32_t fb_get_bpp(void);
uint32_t fb_getpixel(uint32_t x, uint32_t y);
void fb_putpixel(uint32_t x, uint32_t y, uint32_t color);
void fb_draw_rect(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t color);
void fb_fill_screen(uint32_t color);
void fb_clear(uint32_t color);
void fb_draw_char(uint32_t x, uint32_t y, char c, uint32_t fg, uint32_t bg);
void fb_draw_text(uint32_t x, uint32_t y, const char *text, uint32_t fg, uint32_t bg);
void fb_scroll_up(uint32_t pixels, uint32_t bg);

/* VGA text fallback */
void vga_init(void);
void vga_clear(void);
void vga_putc(char c);
void vga_write(const char *s);

/* PS/2 mouse */
void mouse_init(void);
int mouse_get_x(void);
int mouse_get_y(void);

/* Mouse cursor (PNG-based) */
int mouse_cursor_load_from_file(const char *path);
int mouse_cursor_load_from_memory(const unsigned char *png_data, size_t png_size);
void mouse_cursor_get_size(unsigned *w, unsigned *h);
void mouse_cursor_draw(int x, int y);
void mouse_cursor_cleanup(void);
void cursor_setup_assets(void);

/* PCI + USB */
void pci_scan(void);
void usb_init(void);

/* USB HID */
void hid_init(void);
