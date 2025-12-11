// Simple shell UI with input, history, and basic commands.
#include "shell.hpp"

#include <stddef.h>
#include <stdint.h>

#include "renderer.hpp"
#include "drivers/usb/usb_core.hpp"
#include "drivers/usb/include/usb_core.h"
#include "drivers/usb/include/usb_device.h"

namespace {

ShellConfig g_cfg{};

constexpr uint32_t kPadding = 12;
constexpr uint32_t kShellLineHeight = 16;
constexpr uint32_t kMaxInput = 63;
constexpr uint32_t kMaxHistory = 6;

char g_input[kMaxInput + 1]{};
size_t g_input_len = 0;
bool g_cursor_on = true;

char g_history[kMaxHistory][kMaxInput + 1]{};
size_t g_history_len = 0;

size_t str_len(const char* s) {
    size_t len = 0;
    while (s[len] != 0) {
        ++len;
    }
    return len;
}

void copy_str(char* dst, const char* src, size_t dst_cap) {
    if (dst_cap == 0) {
        return;
    }
    size_t i = 0;
    for (; i + 1 < dst_cap && src[i] != 0; ++i) {
        dst[i] = src[i];
    }
    dst[i] = 0;
}

bool str_eq(const char* a, const char* b) {
    size_t i = 0;
    while (a[i] != 0 && b[i] != 0) {
        if (a[i] != b[i]) {
            return false;
        }
        ++i;
    }
    return a[i] == 0 && b[i] == 0;
}

void u32_to_str(uint32_t v, char* out, size_t cap) {
    if (cap == 0) {
        return;
    }
    char buf[10];
    size_t digits = 0;
    do {
        buf[digits++] = static_cast<char>('0' + (v % 10));
        v /= 10;
    } while (v != 0 && digits < sizeof(buf));
    size_t pos = 0;
    while (digits > 0 && pos + 1 < cap) {
        out[pos++] = buf[--digits];
    }
    out[pos] = 0;
}

void describe_usb(char* line, size_t cap) {
    const uint32_t controllers = usb::controller_count();
    const uint32_t devices = usb::device_count();
    char num_buf[12];
    size_t len = 0;
    if (cap == 0) {
        return;
    }
    line[0] = 0;
    const char* prefix = "USB: ";
    for (size_t i = 0; prefix[i] != 0 && len + 1 < cap; ++i) {
        line[len++] = prefix[i];
    }
    line[len] = 0;

    const char* ctl = "controllers=";
    for (size_t i = 0; ctl[i] != 0 && len + 1 < cap; ++i) {
        line[len++] = ctl[i];
    }
    line[len] = 0;

    u32_to_str(controllers, num_buf, sizeof(num_buf));
    for (size_t i = 0; num_buf[i] != 0 && len + 1 < cap; ++i) {
        line[len++] = num_buf[i];
    }
    line[len] = 0;

    const char* dev = " devices=";
    for (size_t i = 0; dev[i] != 0 && len + 1 < cap; ++i) {
        line[len++] = dev[i];
    }
    line[len] = 0;

    u32_to_str(devices, num_buf, sizeof(num_buf));
    for (size_t i = 0; num_buf[i] != 0 && len + 1 < cap; ++i) {
        line[len++] = num_buf[i];
    }
    line[len] = 0;

    // Append first device VID:PID if any.
    if (devices > 0) {
        const usb_device_t* dev = usb_stack_device_at(0);
        if (dev) {
            const usb_device_descriptor_t* ddesc = usb_device_get_descriptor(dev);
            if (ddesc) {
                const char* tail = " vid:pid=";
                for (size_t i = 0; tail[i] != 0 && len + 1 < cap; ++i) {
                    line[len++] = tail[i];
                }
                line[len] = 0;
                u32_to_str(ddesc->idVendor, num_buf, sizeof(num_buf));
                for (size_t i = 0; num_buf[i] != 0 && len + 1 < cap; ++i) {
                    line[len++] = num_buf[i];
                }
                if (len + 1 < cap) {
                    line[len++] = ':';
                }
                u32_to_str(ddesc->idProduct, num_buf, sizeof(num_buf));
                for (size_t i = 0; num_buf[i] != 0 && len + 1 < cap; ++i) {
                    line[len++] = num_buf[i];
                }
                line[len] = 0;
            }
        }
    }
}

void push_history(const char* line) {
    if (str_len(line) == 0) {
        return;
    }
    if (g_history_len == kMaxHistory) {
        for (size_t i = 1; i < kMaxHistory; ++i) {
            copy_str(g_history[i - 1], g_history[i], kMaxInput + 1);
        }
        g_history_len = kMaxHistory - 1;
    }
    copy_str(g_history[g_history_len], line, kMaxInput + 1);
    ++g_history_len;
}

void draw_history() {
    uint32_t y = g_cfg.win_y + 64;
    const uint32_t x = g_cfg.win_x + kPadding;
    for (size_t i = 0; i < g_history_len; ++i) {
        renderer_text(g_history[i], x, y, g_cfg.foreground, g_cfg.window);
        y += kShellLineHeight;
    }
}

void draw_prompt() {
    const uint32_t prompt_x = g_cfg.win_x + kPadding;
    const uint32_t prompt_y = g_cfg.win_y + 40;
    renderer_text("AstraShell>", prompt_x, prompt_y, g_cfg.accent, g_cfg.window);
    uint32_t input_x = prompt_x + 10 * 8 + 8;  // label width (10 chars) + small gap
    renderer_text(g_input, input_x, prompt_y, g_cfg.foreground, g_cfg.window);

    // Draw a simple cursor underscore after current input.
    const uint32_t cx = input_x + static_cast<uint32_t>(g_input_len) * 8;
    char cursor[2] = {g_cursor_on ? '_' : ' ', 0};
    renderer_text(cursor, cx, prompt_y, g_cfg.accent, g_cfg.window);
}

}  // namespace

void shell_init(const ShellConfig& cfg) {
    g_cfg = cfg;
    g_input_len = 0;
    g_history_len = 0;
    g_input[0] = 0;
    shell_render();
}

void shell_handle_key(char key) {
    // Printable ASCII
    if (key >= 32 && key <= 126) {
        if (g_input_len < kMaxInput) {
            g_input[g_input_len++] = key;
            g_input[g_input_len] = 0;
        }
        return;
    }

    // Backspace
    if (key == '\b') {
        if (g_input_len > 0) {
            --g_input_len;
            g_input[g_input_len] = 0;
        }
        return;
    }

    // Enter
    if (key == '\n' || key == '\r') {
        if (g_input_len == 0) {
            return;
        }
        if (str_eq(g_input, "clear")) {
            g_history_len = 0;
        } else if (str_eq(g_input, "help")) {
            push_history("Commands: help, clear, usb");
        } else if (str_eq(g_input, "usb")) {
            char msg[64];
            describe_usb(msg, sizeof(msg));
            push_history(msg);
        } else {
            push_history(g_input);
        }
        g_input_len = 0;
        g_input[0] = 0;
        return;
    }
}

void shell_render() {
    renderer_clear(g_cfg.background);
    renderer_rect(g_cfg.win_x, g_cfg.win_y, g_cfg.win_w, g_cfg.win_h, g_cfg.window);
    renderer_rect(g_cfg.win_x, g_cfg.win_y, g_cfg.win_w, 24, g_cfg.title_bar);
    renderer_rect_outline(g_cfg.win_x, g_cfg.win_y, g_cfg.win_w, g_cfg.win_h, g_cfg.accent);
    renderer_text("AstraOS Shell", g_cfg.win_x + kPadding, g_cfg.win_y + 4, g_cfg.foreground, g_cfg.title_bar);
    draw_history();
    draw_prompt();
}

void shell_blink_tick() {
    g_cursor_on = !g_cursor_on;
    // Redraw only the prompt area cursor.
    draw_prompt();
}

#ifdef HOST_TEST
const char* shell_debug_input() { return g_input; }
size_t shell_debug_input_len() { return g_input_len; }
size_t shell_debug_history_size() { return g_history_len; }
const char* shell_debug_history_line(size_t idx) {
    if (idx >= g_history_len) {
        static const char kEmpty[] = "";
        return kEmpty;
    }
    return g_history[idx];
}
#endif
