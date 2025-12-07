#include "types.h"
#include "drivers.h"
#include "event.h"
#include "kernel.h"
#include "interrupts.h"
#include "kmalloc.h"

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

/* Cursor image support */
static unsigned cursor_w = 0, cursor_h = 0;
static uint32_t *saved_bg = NULL;
static bool saved_valid = false;

static inline bool mouse_byte_ready(void) {
    uint8_t st = inb(PS2_STATUS);
    return (st & 0x21) == 0x21; /* AUX (bit 5) + OBF (bit 0) */
}

static inline int clamp_int(int v, int lo, int hi) {
    return v < lo ? lo : (v > hi ? hi : v);
}

/* Fallback cursor: simple arrow made of a few pixels - bright yellow for visibility */
static void draw_fallback_cursor(int x, int y) {
    uint32_t color = 0xFFFF00; /* Bright yellow - highly visible on blue background */
    
    /* Simple arrow cursor pattern (5x5 pixels):
     *   X
     *   XX
     *   X X
     *   X  X
     *   X   X
     */
    int pattern[][2] = {
        {0, 0},  /* Top */
        {0, 1},  /* Down 1 */
        {1, 1},  /* Right 1, Down 1 */
        {0, 2},  /* Down 2 */
        {2, 2},  /* Right 2, Down 2 */
        {0, 3},  /* Down 3 */
        {3, 3},  /* Right 3, Down 3 */
        {0, 4},  /* Down 4 */
        {4, 4},  /* Right 4, Down 4 */
    };
    
    /* Draw yellow cursor pixels */
    for (int i = 0; i < 9; i++) {
        int px = x + pattern[i][0];
        int py = y + pattern[i][1];
        if (px >= 0 && px < screen_w && py >= 0 && py < screen_h) {
            fb_putpixel((uint32_t)px, (uint32_t)py, color);
        }
    }
}

static void save_bg(int x, int y) {
    if (cursor_w == 0 || cursor_h == 0) return;
    
    /* Allocate buffer for saved background if needed */
    if (!saved_bg) {
        saved_bg = (uint32_t *)kmalloc(cursor_w * cursor_h * sizeof(uint32_t));
        if (!saved_bg) return;
    }
    
    int max_w = screen_w;
    int max_h = screen_h;
    int idx = 0;
    for (unsigned dy = 0; dy < cursor_h; dy++) {
        for (unsigned dx = 0; dx < cursor_w; dx++) {
            int px = x + (int)dx;
            int py = y + (int)dy;
            if (px >= 0 && py >= 0 && px < max_w && py < max_h)
                saved_bg[idx++] = fb_getpixel((uint32_t)px, (uint32_t)py);
            else
                saved_bg[idx++] = 0x000000; /* Black */
        }
    }
    saved_valid = true;
}

static void restore_bg(int x, int y) {
    if (!saved_valid || !saved_bg || cursor_w == 0 || cursor_h == 0) return;
    
    int max_w = screen_w;
    int max_h = screen_h;
    int idx = 0;
    for (unsigned dy = 0; dy < cursor_h; dy++) {
        for (unsigned dx = 0; dx < cursor_w; dx++) {
            int px = x + (int)dx;
            int py = y + (int)dy;
            if (px >= 0 && py >= 0 && px < max_w && py < max_h)
                fb_putpixel((uint32_t)px, (uint32_t)py, saved_bg[idx++]);
            else
                idx++;
        }
    }
}

static void mouse_irq(interrupt_frame_t *f) {
    (void)f;

    /* Check if data is available and from mouse */
    uint8_t st = inb(PS2_STATUS);
    if (!(st & 0x01)) return; /* No data */
    if (!(st & 0x20)) return; /* Not from mouse (aux device) */

    packet[idx++] = inb(PS2_DATA);

    if (idx < 3) return;
    idx = 0;

    int dx = (int8_t)packet[1];
    int dy = (int8_t)packet[2];

    /* Update mouse position */
    int new_x = mouse_x + dx;
    int new_y = mouse_y - dy; /* Y is inverted */

    /* Clamp to screen bounds */
    if (new_x < 0) new_x = 0;
    if (new_y < 0) new_y = 0;
    if (new_x >= screen_w) new_x = screen_w - 1;
    if (new_y >= screen_h) new_y = screen_h - 1;

    /* Only update if position changed */
    if (new_x == mouse_x && new_y == mouse_y) {
        return;
    }

    mouse_x = new_x;
    mouse_y = new_y;

    uint8_t buttons = packet[0] & 0x07;
    uint8_t prev_buttons = last_buttons;

    /* Restore old area, save new, draw cursor */
    if (last_x >= 0 && last_y >= 0) {
        if (cursor_w > 0 && cursor_h > 0) {
            restore_bg(last_x, last_y);
        } else {
            /* Fallback: erase old cursor by drawing background color */
            uint32_t bg_color = 0xFF1E3A5F; /* Shell background color */
            int pattern[][2] = {
                {0, 0}, {0, 1}, {1, 1}, {0, 2}, {2, 2},
                {0, 3}, {3, 3}, {0, 4}, {4, 4},
            };
            for (int i = 0; i < 9; i++) {
                int px = last_x + pattern[i][0];
                int py = last_y + pattern[i][1];
                if (px >= 0 && px < screen_w && py >= 0 && py < screen_h) {
                    fb_putpixel((uint32_t)px, (uint32_t)py, bg_color);
                }
            }
        }
    }
    
    /* Draw cursor at new position */
    if (cursor_w > 0 && cursor_h > 0) {
        save_bg(mouse_x, mouse_y);
        mouse_cursor_draw(mouse_x, mouse_y);
    } else {
        /* Fallback: simple pixel cursor */
        draw_fallback_cursor(mouse_x, mouse_y);
    }
    
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

    /* Load cursor image from assets (if available) */
    /* Example: if you have cursor.png embedded as C array:
     *   extern const unsigned char cursor_png[];
     *   extern const size_t cursor_png_size;
     *   mouse_cursor_load_from_memory(cursor_png, cursor_png_size);
     */

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

    printf("mouse: registering IRQ handler (IRQ12 -> vector 44)\n");
    irq_register_handler(MOUSE_IRQ, mouse_irq);
    printf("mouse: IRQ handler registered\n");

    /* Get cursor size */
    mouse_cursor_get_size(&cursor_w, &cursor_h);
    if (cursor_w == 0 || cursor_h == 0) {
        printf("mouse: WARNING - cursor image not loaded, using fallback\n");
        cursor_w = 0; /* Don't set fake size - use fallback */
        cursor_h = 0;
    } else {
        printf("mouse: cursor image size %ux%u\n", cursor_w, cursor_h);
    }

    /* Center cursor and draw once */
    mouse_x = screen_w / 2;
    mouse_y = screen_h / 2;
    printf("mouse: initial cursor position %d,%d (screen=%dx%d)\n", mouse_x, mouse_y, screen_w, screen_h);
    
    /* Draw cursor - use fallback if PNG cursor not loaded */
    if (cursor_w > 0 && cursor_h > 0) {
        save_bg(mouse_x, mouse_y);
        mouse_cursor_draw(mouse_x, mouse_y);
        printf("mouse: PNG cursor drawn (%ux%u) at %d,%d\n", cursor_w, cursor_h, mouse_x, mouse_y);
    } else {
        draw_fallback_cursor(mouse_x, mouse_y);
        printf("mouse: fallback cursor drawn (yellow arrow) at %d,%d\n", mouse_x, mouse_y);
    }
    
    last_x = mouse_x;
    last_y = mouse_y;
    last_buttons = 0;
    
    /* Force cursor to be visible by drawing it again after a small delay */
    /* This ensures it's drawn after shell initializes */

    printf("PS2: mouse init done\n");
}

/* Public API: get mouse position */
int mouse_get_x(void) {
    return mouse_x;
}

int mouse_get_y(void) {
    return mouse_y;
}
