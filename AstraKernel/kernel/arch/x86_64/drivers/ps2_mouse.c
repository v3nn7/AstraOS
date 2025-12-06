#include "types.h"
#include "drivers.h"
#include "event.h"
#include "kernel.h"
#include "interrupts.h"

#define PS2_DATA   0x60
#define PS2_STATUS 0x64
#define PS2_CMD    0x64

#define MOUSE_IRQ 12

static int mouse_x = 0, mouse_y = 0;
static uint8_t packet[3];
static uint8_t idx = 0;

static int screen_w, screen_h;
static int last_x = -1, last_y = -1;
static uint8_t last_buttons = 0;

static const uint32_t CURSOR_COLOR = 0xFFFFFFFF;
static const uint32_t CURSOR_BG    = 0xFF0F1115; /* matches shell background */
#define CUR_W 5
#define CUR_H 7

static uint32_t saved_bg[CUR_H][CUR_W];
static bool saved_valid = false;

static inline bool mouse_byte_ready(void) {
    uint8_t st = inb(PS2_STATUS);
    return (st & 0x21) == 0x21; /* AUX + OBF */
}

static inline int clamp_int(int v, int lo, int hi) {
    return v < lo ? lo : (v > hi ? hi : v);
}

static void draw_cursor(int x, int y, uint32_t color) {
    /* Tiny 5x7 block cursor */
    for (int dy = 0; dy < 7; ++dy) {
        for (int dx = 0; dx < 5; ++dx) {
            fb_putpixel((uint32_t)(x + dx), (uint32_t)(y + dy), color);
        }
    }
}

static void save_bg(int x, int y) {
    int max_w = screen_w;
    int max_h = screen_h;
    for (int dy = 0; dy < CUR_H; ++dy) {
        for (int dx = 0; dx < CUR_W; ++dx) {
            int px = x + dx;
            int py = y + dy;
            if (px >= 0 && py >= 0 && px < max_w && py < max_h)
                saved_bg[dy][dx] = fb_getpixel((uint32_t)px, (uint32_t)py);
            else
                saved_bg[dy][dx] = CURSOR_BG;
        }
    }
    saved_valid = true;
}

static void restore_bg(int x, int y) {
    if (!saved_valid) return;
    int max_w = screen_w;
    int max_h = screen_h;
    for (int dy = 0; dy < CUR_H; ++dy) {
        for (int dx = 0; dx < CUR_W; ++dx) {
            int px = x + dx;
            int py = y + dy;
            if (px >= 0 && py >= 0 && px < max_w && py < max_h)
                fb_putpixel((uint32_t)px, (uint32_t)py, saved_bg[dy][dx]);
        }
    }
}

static void mouse_irq(interrupt_frame_t *f) {
    (void)f;

    if (!mouse_byte_ready()) return;

    packet[idx++] = inb(PS2_DATA);

    if (idx < 3) return;
    idx = 0;

    int dx = (int8_t)packet[1];
    int dy = (int8_t)packet[2];

    mouse_x += dx;
    mouse_y -= dy;

    if (mouse_x < 0) mouse_x = 0;
    if (mouse_y < 0) mouse_y = 0;
    if (mouse_x >= screen_w) mouse_x = screen_w - 1;
    if (mouse_y >= screen_h) mouse_y = screen_h - 1;

    uint8_t buttons = packet[0] & 0x07;

    uint8_t prev_buttons = last_buttons;

    /* Restore old area, save new, draw cursor */
    if (last_x >= 0 && last_y >= 0)
        restore_bg(last_x, last_y);
    save_bg(mouse_x, mouse_y);
    draw_cursor(mouse_x, mouse_y, CURSOR_COLOR);
    last_x = mouse_x;
    last_y = mouse_y;
    last_buttons = buttons;

    /* Push button event on change */
    if (buttons != prev_buttons) {
        gui_event_push_mouse_button(mouse_x, mouse_y, buttons);
    }
    gui_event_push_mouse_move(mouse_x, mouse_y, dx, dy, buttons);
}

void mouse_init(void) {
    screen_w = fb_width();
    screen_h = fb_height();
    last_x = -1;
    last_y = -1;
    last_buttons = 0;
    saved_valid = false;

    /* Flush pending output bytes */
    while (inb(PS2_STATUS) & 0x01) (void)inb(PS2_DATA);

    // Enable auxiliary
    outb(PS2_CMD, 0xA8);

    // Enable mouse IRQ
    outb(PS2_CMD, 0x20);
    uint8_t cfg = inb(PS2_DATA);
    cfg |= 2; // enable IRQ12
    outb(PS2_CMD, 0x60);
    outb(PS2_DATA, cfg);

    // Default settings
    outb(PS2_CMD, 0xD4);
    outb(PS2_DATA, 0xF6); // default settings
    (void)inb(PS2_DATA);

    // Enable data reporting
    outb(PS2_CMD, 0xD4);
    outb(PS2_DATA, 0xF4);
    (void)inb(PS2_DATA);

    // Unmask IRQ12
    uint8_t mask = inb(0xA1);
    mask &= ~(1 << 4);
    outb(0xA1, mask);

    irq_register_handler(MOUSE_IRQ, mouse_irq);

    /* Center cursor and draw once */
    mouse_x = screen_w / 2;
    mouse_y = screen_h / 2;
    save_bg(mouse_x, mouse_y);
    draw_cursor(mouse_x, mouse_y, CURSOR_COLOR);
    last_x = mouse_x;
    last_y = mouse_y;
    last_buttons = 0;

    printf("PS2: mouse init done\n");
}
