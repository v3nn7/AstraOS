#include "logger.hpp"

#include "../drivers/serial.hpp"

extern "C" void serial_write(const char* s);
extern "C" void serial_init(void);

namespace {
uint32_t g_cursor_x = 12;
uint32_t g_cursor_y = 40;
const uint32_t kLineHeight = 16;
Rgb g_bg{18, 18, 24};
Rgb g_fg{120, 200, 255};
}  // namespace

extern "C" void logger_init() {
    serial_init();
    g_cursor_x = 12;
    g_cursor_y = 40;
}

static void framebuffer_puts(const char* s) {
#ifndef HOST_TEST
    const uint32_t screen_h = renderer_height();
    const uint32_t screen_w = renderer_width();
    const uint32_t log_width = (screen_w > 320) ? 320u : (screen_w / 2);
    // Auto-scroll: when near bottom, clear only the log column to avoid touching shell.
    if (screen_h > kLineHeight * 4 && g_cursor_y + kLineHeight > screen_h) {
        renderer_rect(0, 0, log_width, screen_h, g_bg);
        g_cursor_x = 12;
        g_cursor_y = 40;
    }
#endif
    renderer_text(s, g_cursor_x, g_cursor_y, g_fg, g_bg);
    g_cursor_y += kLineHeight;
}

extern "C" void klog(const char* msg) {
    serial_write("[LOG] ");
    serial_write(msg);
    serial_write("\r\n");
    framebuffer_puts(msg);
}

extern "C" [[noreturn]] void kpanic(const char* msg) {
    serial_write("[PANIC] ");
    serial_write(msg);
    serial_write("\r\n");

    renderer_clear(Rgb{40, 0, 0});
    renderer_text("PANIC", 12, 12, Rgb{255, 180, 180}, Rgb{40, 0, 0});
    renderer_text(msg, 12, 32, Rgb{255, 255, 255}, Rgb{40, 0, 0});
    while (true) {
        __asm__ __volatile__("cli; hlt");
    }
}
