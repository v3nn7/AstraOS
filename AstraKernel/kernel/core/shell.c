#include "types.h"
#include "drivers.h"
#include "kernel.h"
#include "string.h"
#include "installer.h"
#include "input/input_core.h"

#define CHAR_W 8
#define CHAR_H 16
#define SHELL_LEFT 8
#define SHELL_TOP 32
#define CMD_MAX 256
#define HISTORY_MAX 32

static uint32_t cursor_x = SHELL_LEFT;
static uint32_t cursor_y = SHELL_TOP;

static const uint32_t COLOR_BG     = 0xFF0F1115;
static const uint32_t COLOR_BAR    = 0xFF1E3A5F;
static const uint32_t COLOR_PROMPT = 0xFF7AD17A;
static const uint32_t COLOR_TEXT   = 0xFFE0E0E0;
static const uint32_t COLOR_SHADOW = 0xAA000000;

static char cmd_buf[CMD_MAX];
static int cmd_len = 0;
static int cursor_pos = 0; /* Cursor position in command buffer */

/* HISTORY */
static char history[HISTORY_MAX][CMD_MAX];
static int history_len = 0;
static int history_pos = -1;

/* ============================ CORE DRAWING ============================ */

static void scroll() {
    uint32_t h = fb_height();
    if (cursor_y + CHAR_H < h) return;

    fb_scroll_up(CHAR_H, COLOR_BG);
    cursor_y -= CHAR_H;
}

static void new_line() {
    cursor_x = SHELL_LEFT;
    cursor_y += CHAR_H;
    scroll();
}

static void putc_fb(char c) {
    if (c == '\n') { new_line(); return; }

    fb_draw_char(cursor_x, cursor_y, c, COLOR_TEXT, COLOR_BG);
    cursor_x += CHAR_W;

    if (cursor_x + CHAR_W >= fb_width())
        new_line();
}

static void print(const char *s) {
    while (*s) putc_fb(*s++);
}

/* ============================ header ============================ */

static void header_bar() {
    uint32_t w = fb_width();
    fb_draw_rect(0, 0, w, 24, COLOR_BAR);
    fb_draw_rect(0, 24, w, 2, COLOR_SHADOW);

    cursor_x = 8;
    cursor_y = 4;

    const char *title = "AstraOS Shell v3 (framebuffer)";
    while (*title)
        fb_draw_char(cursor_x, cursor_y, *title++, 0xFFFFFFFF, COLOR_BAR);

    cursor_x = SHELL_LEFT;
    cursor_y = SHELL_TOP;
}

/* ============================ backspace ============================ */

static void backspace() {
    if (cursor_pos <= 0) return;

    /* Shift characters left */
    for (int i = cursor_pos - 1; i < cmd_len; i++) {
        cmd_buf[i] = cmd_buf[i + 1];
    }
    cmd_len--;
    cursor_pos--;

    /* Redraw line */
    cursor_x = SHELL_LEFT + CHAR_W;
    for (uint32_t i = 0; i < fb_width() / CHAR_W; i++)
        fb_draw_char(cursor_x + i * CHAR_W, cursor_y, ' ', COLOR_TEXT, COLOR_BG);
    cursor_x = SHELL_LEFT + CHAR_W;
    for (int i = 0; i < cmd_len; i++) {
        fb_draw_char(cursor_x + i * CHAR_W, cursor_y, cmd_buf[i], COLOR_TEXT, COLOR_BG);
    }
    cursor_x = SHELL_LEFT + CHAR_W + cursor_pos * CHAR_W;
}

/* ============================ prompt ============================ */

static void prompt() {
    cursor_x = SHELL_LEFT;
    putc_fb('>');
    fb_draw_char(cursor_x, cursor_y, ' ', COLOR_PROMPT, COLOR_BG);
    cursor_x += CHAR_W;
}

/* ============================ HISTORY ============================ */

static void history_add(const char *cmd) {
    if (cmd_len == 0) return;
    if (history_len < HISTORY_MAX) {
        strcpy(history[history_len++], cmd);
    } else {
        for (int i = 1; i < HISTORY_MAX; i++)
            strcpy(history[i - 1], history[i]);
        strcpy(history[HISTORY_MAX - 1], cmd);
    }
}

static void history_load(int idx) {
    cmd_len = strlen(history[idx]);
    strcpy(cmd_buf, history[idx]);
    cursor_pos = cmd_len; /* Cursor at end */

    cursor_x = SHELL_LEFT;
    for (uint32_t i = 0; i < fb_width() / CHAR_W; i++)
        fb_draw_char(cursor_x + i * CHAR_W, cursor_y, ' ', COLOR_TEXT, COLOR_BG);

    cursor_x = SHELL_LEFT + CHAR_W; // after "> "
    for (int i = 0; i < cmd_len; i++)
        fb_draw_char(cursor_x + i * CHAR_W, cursor_y, cmd_buf[i], COLOR_TEXT, COLOR_BG);

    cursor_x = SHELL_LEFT + CHAR_W + cursor_pos * CHAR_W;
}

/* ============================ AUTOCOMPLETE ============================ */

static const char *commands[] = {
    "help",
    "clear",
    "cls",
    "about",
    "install",
    "echo",
    "history",
    "ls",
    "reboot",
    0
};

static const char *autocomplete(const char *in) {
    int len = strlen(in);
    if (len == 0) return 0;

    for (int i = 0; commands[i]; i++) {
        if (strncmp(in, commands[i], len) == 0)
            return commands[i];
    }
    return 0;
}

/* ============================ COMMAND EXECUTION ============================ */

static void run_command() {
    cmd_buf[cmd_len] = 0;

    if (cmd_len == 0) {
        print("\n");
        return;
    }

    history_add(cmd_buf);

    if (strcmp(cmd_buf, "help") == 0) {
        print("Commands:\n");
        print("  help     - list commands\n");
        print("  clear    - clear screen (alias: cls)\n");
        print("  about    - info\n");
        print("  install  - run installer\n");
        print("  echo X   - print X\n");
        print("  history  - show history\n");
        print("  ls       - list stub entries\n");
        print("  reboot   - halt (stub)\n");
    }

    else if (strcmp(cmd_buf, "clear") == 0 || strcmp(cmd_buf, "cls") == 0) {
        fb_fill_screen(COLOR_BG);
        header_bar();
    }

    else if (strcmp(cmd_buf, "about") == 0) {
        print("AstraOS Shell v3\n");
    }

    else if (strcmp(cmd_buf, "install") == 0) {
        print("Launching AstraInstaller...\n");
        installer_run();
    }

    else if (strncmp(cmd_buf, "echo ", 5) == 0) {
        print(cmd_buf + 5);
        print("\n");
    }

    else if (strcmp(cmd_buf, "history") == 0) {
        for (int i = 0; i < history_len; i++) {
            print(history[i]);
            print("\n");
        }
    }

    else if (strcmp(cmd_buf, "ls") == 0) {
        print(".\n..\nboot/\nsys/\ninitrd.img (stub)\n");
    }

    else if (strcmp(cmd_buf, "reboot") == 0) {
        print("System halt (stub)\n");
        for(;;) __asm__ volatile("hlt");
    }

    else {
        print("Unknown command: ");
        print(cmd_buf);
        print("\n");
    }
}

/* ============================ MAIN LOOP ============================ */

void shell_run() {
    printf("shell: starting, fb_w=%u fb_h=%u\n", fb_width(), fb_height());
    
    /* Test: fill screen with a visible color first */
    fb_fill_screen(0xFF1E3A5F); /* Dark blue instead of black */
    
    printf("shell: screen filled\n");
    
    header_bar();
    printf("shell: header drawn\n");

    print("Welcome to AstraOS Shell v3!\n");
    prompt();
    
    printf("shell: ready, entering main loop\n");

    cmd_len = 0;
    cursor_pos = 0;
    history_pos = -1;

    /* Test: draw a test pixel to verify framebuffer works */
    {
        extern void fb_putpixel(uint32_t x, uint32_t y, uint32_t color);
        for (int i = 0; i < 100; i++) {
            fb_putpixel((uint32_t)(50 + i), 50, 0xFFFFFFFF); /* White line */
        }
        printf("shell: test white line drawn at y=50\n");
    }
    
    /* Test: draw a big white square where cursor should be */
    {
        extern void fb_putpixel(uint32_t x, uint32_t y, uint32_t color);
        extern uint32_t fb_width(void);
        extern uint32_t fb_height(void);
        uint32_t w = fb_width();
        uint32_t h = fb_height();
        int cx = w / 2;
        int cy = h / 2;
        printf("shell: drawing test square at center (%d,%d)\n", cx, cy);
        for (int dy = -10; dy <= 10; dy++) {
            for (int dx = -10; dx <= 10; dx++) {
                int px = cx + dx;
                int py = cy + dy;
                if (px >= 0 && px < (int)w && py >= 0 && py < (int)h) {
                    fb_putpixel((uint32_t)px, (uint32_t)py, 0xFFFF0000); /* Red square */
                }
            }
        }
    }
    
    /* Track previous mouse position to avoid unnecessary updates */
    static int prev_mx = -1;
    static int prev_my = -1;
    
    /* Poll counter to reduce USB polling frequency */
    static uint32_t poll_counter = 0;
    
    while (1) {
        /* Poll USB HID devices more frequently for better responsiveness */
        poll_counter++;
        if (poll_counter % 5 == 0) { /* Poll every 5 iterations instead of 10 */
            extern void usb_hid_poll_mouse(void);
            extern void usb_hid_poll_keyboard(void);
            extern bool usb_hid_mouse_available(void);
            extern bool usb_hid_keyboard_available(void);
            
            /* Only poll if devices are available */
            if (usb_hid_keyboard_available()) {
                usb_hid_poll_keyboard();
            }
            if (usb_hid_mouse_available()) {
                usb_hid_poll_mouse();
            }
        }
        
        /* Update mouse cursor only when position changed (every 5 iterations) */
        if (poll_counter % 5 == 0) {
            extern void mouse_cursor_update(void);
            extern bool mouse_cursor_needs_redraw(void);
            extern int mouse_get_x(void);
            extern int mouse_get_y(void);
            
            int mx = mouse_get_x();
            int my = mouse_get_y();
            
            /* Update cursor only if position changed or needs redraw */
            if ((mx != prev_mx || my != prev_my || mouse_cursor_needs_redraw()) && mx >= 0 && my >= 0) {
                mouse_cursor_update();
                prev_mx = mx;
                prev_my = my;
            }
        }
        
        /* Poll input events (supports both ASCII chars and keycodes) */
        input_event_t event;
        bool has_input = false;
        char ch = 0;
        uint32_t keycode = 0;
        
        /* Try to get input event */
        if (input_event_poll(&event)) {
            has_input = true;
            
            if (event.type == INPUT_EVENT_KEY_CHAR) {
                /* ASCII character event */
                ch = event.key.ascii;
            } else if (event.type == INPUT_EVENT_KEY_PRESS) {
                /* Key press event (for special keys like arrows) */
                keycode = event.key.keycode;
            } else {
                /* Other event types (mouse, etc.) - ignore for shell */
                has_input = false;
            }
        }
        
        /* Fallback: try legacy keyboard_read_char for compatibility */
        if (!has_input) {
            if (keyboard_read_char(&ch)) {
                has_input = true;
            }
        }
        
        if (!has_input) {
            /* No input available - small delay to avoid busy loop */
            for (volatile int i = 0; i < 10000; i++);
            continue;
        }

        /* --- Handle keycode events (special keys) --- */
        
        if (keycode != 0) {
            /* Arrow Up (HID usage 0x52) */
            if (keycode == 0x52) {
                if (history_len > 0) {
                    if (history_pos < history_len - 1)
                        history_pos++;
                    history_load(history_len - 1 - history_pos);
                }
                continue;
            }
            
            /* Arrow Down (HID usage 0x51) */
            if (keycode == 0x51) {
                if (history_pos > 0) {
                    history_pos--;
                    history_load(history_len - 1 - history_pos);
                } else {
                    /* Clear command line */
                    cmd_len = 0;
                    cursor_pos = 0;
                    cmd_buf[0] = 0;
                    cursor_x = SHELL_LEFT;
                    for (uint32_t i = 0; i < fb_width() / CHAR_W; i++)
                        fb_draw_char(cursor_x + i * CHAR_W, cursor_y, ' ', COLOR_TEXT, COLOR_BG);
                    cursor_x = SHELL_LEFT + CHAR_W;
                }
                continue;
            }
            
            /* Arrow Left (HID usage 0x50) - move cursor left */
            if (keycode == 0x50) {
                if (cursor_pos > 0) {
                    cursor_pos--;
                    cursor_x -= CHAR_W;
                }
                continue;
            }
            
            /* Arrow Right (HID usage 0x4F) - move cursor right */
            if (keycode == 0x4F) {
                if (cursor_pos < cmd_len) {
                    cursor_pos++;
                    cursor_x += CHAR_W;
                }
                continue;
            }
            
            /* Home (HID usage 0x4A) - move to start */
            if (keycode == 0x4A) {
                cursor_pos = 0;
                cursor_x = SHELL_LEFT + CHAR_W;
                continue;
            }
            
            /* End (HID usage 0x4D) - move to end */
            if (keycode == 0x4D) {
                cursor_pos = cmd_len;
                cursor_x = SHELL_LEFT + CHAR_W + cursor_pos * CHAR_W;
                continue;
            }
            
            /* Delete (HID usage 0x4C) - delete character at cursor */
            if (keycode == 0x4C) {
                if (cursor_pos < cmd_len) {
                    /* Shift characters left */
                    for (int i = cursor_pos; i < cmd_len; i++) {
                        cmd_buf[i] = cmd_buf[i + 1];
                    }
                    cmd_len--;
                    /* Redraw line */
                    cursor_x = SHELL_LEFT + CHAR_W;
                    for (uint32_t i = 0; i < fb_width() / CHAR_W; i++)
                        fb_draw_char(cursor_x + i * CHAR_W, cursor_y, ' ', COLOR_TEXT, COLOR_BG);
                    cursor_x = SHELL_LEFT + CHAR_W;
                    for (int i = 0; i < cmd_len; i++) {
                        fb_draw_char(cursor_x + i * CHAR_W, cursor_y, cmd_buf[i], COLOR_TEXT, COLOR_BG);
                    }
                    cursor_x = SHELL_LEFT + CHAR_W + cursor_pos * CHAR_W;
                }
                continue;
            }
            
            /* Ignore other keycodes without ASCII representation */
            continue;
        }

        /* --- Handle ASCII character events --- */

        if (ch == '\b') { // backspace
            backspace();
            continue;
        }

        /* ENTER */
        if (ch == '\n' || ch == '\r') {
            print("\n");
            run_command();
            cmd_len = 0;
            cursor_pos = 0;
            cmd_buf[0] = 0;
            history_pos = -1;
            prompt();
            continue;
        }

        /* TAB → autocomplete */
        if (ch == '\t') {
            const char *match = autocomplete(cmd_buf);
            if (match) {
                int old = cmd_len;
                int full = strlen(match);
                for (int i = old; i < full; i++) {
                    cmd_buf[cmd_len++] = match[i];
                    putc_fb(match[i]);
                }
            }
            continue;
        }

        /* --- normal characters --- */

        if (cmd_len < CMD_MAX - 1) {
            /* Insert character at cursor position */
            if (cursor_pos < cmd_len) {
                /* Shift characters right */
                for (int i = cmd_len; i > cursor_pos; i--) {
                    cmd_buf[i] = cmd_buf[i - 1];
                }
            }
            cmd_buf[cursor_pos] = ch;
            cmd_len++;
            cursor_pos++;
            
            /* Redraw line */
            cursor_x = SHELL_LEFT + CHAR_W;
            for (uint32_t i = 0; i < fb_width() / CHAR_W; i++)
                fb_draw_char(cursor_x + i * CHAR_W, cursor_y, ' ', COLOR_TEXT, COLOR_BG);
            cursor_x = SHELL_LEFT + CHAR_W;
            for (int i = 0; i < cmd_len; i++) {
                fb_draw_char(cursor_x + i * CHAR_W, cursor_y, cmd_buf[i], COLOR_TEXT, COLOR_BG);
            }
            cursor_x = SHELL_LEFT + CHAR_W + cursor_pos * CHAR_W;
        }
    }
}
