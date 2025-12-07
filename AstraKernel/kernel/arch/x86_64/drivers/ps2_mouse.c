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

static inline bool wait_input_clear_mouse(void) {
    for (int i = 0; i < 100000; i++) {
        if (!(inb(PS2_STATUS) & 0x02)) return true;
    }
    return false;
}

static inline bool wait_output_ready_mouse(void) {
    for (int i = 0; i < 100000; i++) {
        if (inb(PS2_STATUS) & 0x01) return true;
    }
    return false;
}

void mouse_init(void) {
    printf("mouse: initializing\n");
    screen_w = fb_width();
    screen_h = fb_height();
    printf("mouse: screen size %dx%d\n", screen_w, screen_h);
    last_x = -1;
    last_y = -1;
    last_buttons = 0;
    saved_valid = false;

    /* Flush pending output bytes */
    int flush_count = 0;
    while ((inb(PS2_STATUS) & 0x01) && flush_count < 100) {
        (void)inb(PS2_DATA);
        flush_count++;
    }
    printf("mouse: flushed %d bytes\n", flush_count);

    // Enable auxiliary port
    printf("mouse: enabling auxiliary port\n");
    if (!wait_input_clear_mouse()) {
        printf("mouse: timeout enabling auxiliary port\n");
        return;
    }
    outb(PS2_CMD, 0xA8);

    // Enable mouse IRQ in controller config
    printf("mouse: configuring controller\n");
    if (!wait_input_clear_mouse()) {
        printf("mouse: timeout reading config\n");
        return;
    }
    outb(PS2_CMD, 0x20);
    if (!wait_output_ready_mouse()) {
        printf("mouse: timeout waiting for config response\n");
        return;
    }
    uint8_t cfg = inb(PS2_DATA);
    cfg |= 2; // enable IRQ12
    if (!wait_input_clear_mouse()) {
        printf("mouse: timeout writing config\n");
        return;
    }
    outb(PS2_CMD, 0x60);
    if (!wait_input_clear_mouse()) {
        printf("mouse: timeout writing config data\n");
        return;
    }
    outb(PS2_DATA, cfg);
    printf("mouse: config updated (cfg=0x%02x)\n", cfg);

    // Default settings - skip if fails
    printf("mouse: setting default settings\n");
    if (wait_input_clear_mouse()) {
        outb(PS2_CMD, 0xD4);
        if (wait_input_clear_mouse()) {
            outb(PS2_DATA, 0xF6); // default settings
            if (wait_output_ready_mouse()) {
                (void)inb(PS2_DATA);
                printf("mouse: default settings applied\n");
            }
        }
    }

    // Enable data reporting - skip if fails
    printf("mouse: enabling data reporting\n");
    if (wait_input_clear_mouse()) {
        outb(PS2_CMD, 0xD4);
        if (wait_input_clear_mouse()) {
            outb(PS2_DATA, 0xF4);
            if (wait_output_ready_mouse()) {
                (void)inb(PS2_DATA);
                printf("mouse: data reporting enabled\n");
            }
        }
    }

    // Skip PIC unmask in APIC mode - IOAPIC handles IRQ routing
    printf("mouse: skipping PIC unmask (using APIC/IOAPIC)\n");

    printf("mouse: registering IRQ handler\n");
    irq_register_handler(MOUSE_IRQ, mouse_irq);
    printf("mouse: IRQ handler registered\n");

    /* Center cursor and draw once */
    mouse_x = screen_w / 2;
    mouse_y = screen_h / 2;
    printf("mouse: initial cursor position %d,%d\n", mouse_x, mouse_y);
    save_bg(mouse_x, mouse_y);
    draw_cursor(mouse_x, mouse_y, CURSOR_COLOR);
    last_x = mouse_x;
    last_y = mouse_y;
    last_buttons = 0;

    printf("PS2: mouse init done\n");
}
