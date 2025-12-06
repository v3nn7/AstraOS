#pragma once

#include "types.h"
#include "interrupts.h"

void keyboard_init(void);
bool keyboard_has_data(void);
bool keyboard_pop_scancode(uint8_t *code_out);
bool keyboard_pop_char(char *ch_out);

void fb_init(uint64_t addr, uint32_t width, uint32_t height, uint32_t pitch, uint32_t bpp);
uint32_t fb_width(void);
uint32_t fb_height(void);
void fb_putpixel(uint32_t x, uint32_t y, uint32_t color);
void fb_draw_rect(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t color);
void fb_fill_screen(uint32_t color);
void fb_draw_char(uint32_t x, uint32_t y, char c, uint32_t fg, uint32_t bg);
void fb_draw_text(uint32_t x, uint32_t y, const char *text, uint32_t fg, uint32_t bg);
