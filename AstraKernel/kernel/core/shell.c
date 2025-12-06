#include "types.h"
#include "drivers.h"
#include "kernel.h"

static uint32_t cursor_x = 8;
static uint32_t cursor_y = 32; /* leave space for header bar */
static const uint32_t color_bg       = 0xFF0F1115;
static const uint32_t color_bar      = 0xFF1E3A5F;
static const uint32_t color_prompt   = 0xFF7AD17A;
static const uint32_t color_text     = 0xFFE0E0E0;
static const uint32_t color_shadow   = 0xAA000000;

static void putc_fb(char c) {
    if (c == '\n') {
        cursor_x = 8;
        cursor_y += 16;
        return;
    }
    fb_draw_char(cursor_x, cursor_y, c, color_text, color_bg);
    cursor_x += 8;
    if (cursor_x + 8 >= fb_width()) {
        cursor_x = 8;
        cursor_y += 16;
    }
}

static void print(const char *s) {
    while (*s) putc_fb(*s++);
}

static void backspace(void) {
    if (cursor_x >= 8 + 8) {
        cursor_x -= 8;
        fb_draw_char(cursor_x, cursor_y, ' ', color_text, color_bg);
    }
}

static void header_bar(void) {
    uint32_t w = fb_width();
    fb_draw_rect(0, 0, w, 24, color_bar);
    fb_draw_rect(0, 24, w, 2, color_shadow);
    cursor_x = 8;
    cursor_y = 4;
    const char *title = "AstraOS framebuffer shell";
    while (*title) fb_draw_char(cursor_x, cursor_y, *title++, 0xFFFFFFFF, color_bar);
    cursor_x = 8;
    cursor_y = 32;
}

static void prompt(void) {
    fb_draw_char(cursor_x, cursor_y, '>', color_prompt, color_bg);
    cursor_x += 8;
    fb_draw_char(cursor_x, cursor_y, ' ', color_prompt, color_bg);
    cursor_x += 8;
}

void shell_run(void) {
    fb_fill_screen(color_bg);
    header_bar();
    print("AstraOS shell (framebuffer)\n");
    prompt();

    while (1) {
        char ch;
        if (!keyboard_pop_char(&ch)) continue;
        if (ch == '\n' || ch == '\r') {
            print("\n");
            prompt();
            continue;
        }
        if (ch == '\b') {
            backspace();
            continue;
        }
        putc_fb(ch);
    }
}
