#include "types.h"
#include "drivers.h"
#include "kernel.h"
#include "string.h"

#define CHAR_W 8
#define CHAR_H 16
#define SHELL_LEFT 8
#define SHELL_TOP 32
#define CMD_MAX 256

static uint32_t cursor_x = SHELL_LEFT;
static uint32_t cursor_y = SHELL_TOP;

static const uint32_t COLOR_BG     = 0xFF0F1115;
static const uint32_t COLOR_BAR    = 0xFF1E3A5F;
static const uint32_t COLOR_PROMPT = 0xFF7AD17A;
static const uint32_t COLOR_TEXT   = 0xFFE0E0E0;
static const uint32_t COLOR_SHADOW = 0xAA000000;

static char cmd_buf[CMD_MAX];
static int cmd_len = 0;

/* ---------------------------------------------------
   MOVE CURSOR + SCROLL
--------------------------------------------------- */

static void scroll() {
    uint32_t h = fb_height();
    if (cursor_y + CHAR_H < h) return;

    // Scroll everything 1 line up
    fb_scroll_up(CHAR_H, COLOR_BG);
    cursor_y -= CHAR_H;
}

static void new_line() {
    cursor_x = SHELL_LEFT;
    cursor_y += CHAR_H;
    scroll();
}

/* ---------------------------------------------------
   DRAW CHAR
--------------------------------------------------- */

static void putc_fb(char c) {
    if (c == '\n') {
        new_line();
        return;
    }
    fb_draw_char(cursor_x, cursor_y, c, COLOR_TEXT, COLOR_BG);
    cursor_x += CHAR_W;

    if (cursor_x + CHAR_W >= fb_width()) {
        new_line();
    }
}

/* ---------------------------------------------------
   BASIC PRINT
--------------------------------------------------- */

static void print(const char *s) {
    while (*s) putc_fb(*s++);
}

/* ---------------------------------------------------
   BACKSPACE
--------------------------------------------------- */

static void backspace() {
    if (cmd_len > 0) {
        cmd_len--;
        if (cursor_x >= SHELL_LEFT + CHAR_W) {
            cursor_x -= CHAR_W;
        }
        fb_draw_char(cursor_x, cursor_y, ' ', COLOR_TEXT, COLOR_BG);
    }
}

/* ---------------------------------------------------
   HEADER BAR
--------------------------------------------------- */

static void header_bar() {
    uint32_t w = fb_width();
    fb_draw_rect(0, 0, w, 24, COLOR_BAR);
    fb_draw_rect(0, 24, w, 2, COLOR_SHADOW);

    cursor_x = 8;
    cursor_y = 4;

    const char *title = "AstraOS framebuffer shell";
    while (*title)
        fb_draw_char(cursor_x, cursor_y, *title++, 0xFFFFFFFF, COLOR_BAR);

    cursor_x = SHELL_LEFT;
    cursor_y = SHELL_TOP;
}

/* ---------------------------------------------------
   PARSE COMMANDS
--------------------------------------------------- */

static void run_command() {
    cmd_buf[cmd_len] = 0;

    if (cmd_len == 0) {
        print("\n");
        return;
    }

    if (strcmp(cmd_buf, "help") == 0) {
        print("Commands:\n");
        print("  help  - list commands\n");
        print("  clear - clear screen\n");
        print("  about - info\n");
    }

    else if (strcmp(cmd_buf, "clear") == 0) {
        fb_fill_screen(COLOR_BG);
        header_bar();
    }

    else if (strcmp(cmd_buf, "about") == 0) {
        print("AstraOS shell v0.0.1\n");
    }

    else {
        print("Unknown command: ");
        print(cmd_buf);
        print("\n");
    }
}

/* ---------------------------------------------------
   PROMPT
--------------------------------------------------- */

static void prompt() {
    cursor_x = SHELL_LEFT;
    putc_fb('>');
    fb_draw_char(cursor_x, cursor_y, ' ', COLOR_PROMPT, COLOR_BG);
    cursor_x += CHAR_W;
}

/* ---------------------------------------------------
   MAIN SHELL LOOP
--------------------------------------------------- */

void shell_run() {
    fb_fill_screen(COLOR_BG);
    header_bar();

    print("AstraOS shell (framebuffer)\n");
    prompt();

    cmd_len = 0;

    while (1) {
        char ch;
        if (!keyboard_pop_char(&ch)) continue;

        if (ch == '\b') {
            backspace();
            continue;
        }

        if (ch == '\n' || ch == '\r') {
            print("\n");
            run_command();
            cmd_len = 0;
            prompt();
            continue;
        }

        if (cmd_len < CMD_MAX - 1) {
            cmd_buf[cmd_len++] = ch;
            putc_fb(ch);
        }
    }
}
