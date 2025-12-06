#include "types.h"
#include "drivers.h"
#include "kernel.h"
#include "string.h"
#include "installer.h"

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
    if (cmd_len <= 0) return;

    cmd_len--;

    if (cursor_x >= SHELL_LEFT + CHAR_W)
        cursor_x -= CHAR_W;

    fb_draw_char(cursor_x, cursor_y, ' ', COLOR_TEXT, COLOR_BG);
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

    cursor_x = SHELL_LEFT;
    for (uint32_t i = 0; i < fb_width() / CHAR_W; i++)
        fb_draw_char(cursor_x + i * CHAR_W, cursor_y, ' ', COLOR_TEXT, COLOR_BG);

    cursor_x = SHELL_LEFT + CHAR_W; // after "> "
    for (int i = 0; i < cmd_len; i++)
        fb_draw_char(cursor_x + i * CHAR_W, cursor_y, cmd_buf[i], COLOR_TEXT, COLOR_BG);

    cursor_x = SHELL_LEFT + CHAR_W + cmd_len * CHAR_W;
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
    fb_fill_screen(COLOR_BG);
    header_bar();

    print("Welcome to AstraOS Shell v3!\n");
    prompt();

    cmd_len = 0;
    history_pos = -1;

    while (1) {

        char ch;
        if (!keyboard_pop_char(&ch))
            continue;

        /* --- special keys --- */

        if (ch == '\b') { // backspace
            backspace();
            if (cmd_len >= 0) cmd_buf[cmd_len] = 0;
            continue;
        }

        /* ENTER */
        if (ch == '\n' || ch == '\r') {
            print("\n");
            run_command();
            cmd_len = 0;
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

        unsigned char code = (unsigned char)ch;

        /* UP arrow (0x80 marker — you disabled extended keys so using hack) */
        if (code == 0x80) {  // fake code — replace when you add real arrow keys
            if (history_len > 0) {
                if (history_pos < history_len - 1)
                    history_pos++;

                history_load(history_len - 1 - history_pos);
            }
            continue;
        }

        /* DOWN arrow (fake for now) */
        if (code == 0x81) {
            if (history_pos > 0) {
                history_pos--;
                history_load(history_len - 1 - history_pos);
            } else {
                // clear
                cmd_len = 0;
                cmd_buf[0] = 0;

                cursor_x = SHELL_LEFT;
                for (uint32_t i = 0; i < fb_width() / CHAR_W; i++)
                    fb_draw_char(cursor_x + i * CHAR_W, cursor_y, ' ', COLOR_TEXT, COLOR_BG);

                cursor_x = SHELL_LEFT + CHAR_W;
            }
            continue;
        }

        /* --- normal characters --- */

        if (cmd_len < CMD_MAX - 1) {
            cmd_buf[cmd_len++] = ch;
            putc_fb(ch);
        }
    }
}
