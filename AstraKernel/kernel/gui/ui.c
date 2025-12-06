#include "ui.h"
#include "drivers.h"
#include "string.h"

void ui_clear(uint32_t color_bg) {
    fb_fill_screen(color_bg);
}

void ui_rect(int x, int y, int w, int h, uint32_t color) {
    fb_draw_rect((uint32_t)x, (uint32_t)y, (uint32_t)w, (uint32_t)h, color);
}

void ui_text(int x, int y, const char *txt, uint32_t fg, uint32_t bg) {
    fb_draw_text((uint32_t)x, (uint32_t)y, txt, fg, bg);
}

static bool point_in(int px, int py, int x, int y, int w, int h) {
    return (px >= x && py >= y && px < x + w && py < y + h);
}

bool ui_button(ui_button_t *btn, gui_event_t *ev, uint32_t color, uint32_t color_text) {
    ui_rect(btn->x, btn->y, btn->w, btn->h, color);
    int tx = btn->x + 6;
    int ty = btn->y + (btn->h / 2 - 4);
    ui_text(tx, ty, btn->text, color_text, color);

    if (!ev) return false;
    if (ev->type == GUI_EVENT_MOUSE_BUTTON && (ev->mouse.buttons & 1)) {
        if (point_in(ev->mouse.x, ev->mouse.y, btn->x, btn->y, btn->w, btn->h)) {
            return true;
        }
    }
    return false;
}

bool ui_listview(ui_listview_t *lv, gui_event_t *ev, int x, int y, int w, int h,
                 uint32_t color_bg, uint32_t color_sel, uint32_t color_text) {
    ui_rect(x, y, w, h, color_bg);
    int line_h = 14;
    int max_visible = h / line_h;
    lv->visible = max_visible;
    if (lv->scroll < 0) lv->scroll = 0;
    if (lv->scroll > lv->count - max_visible) lv->scroll = lv->count - max_visible;
    if (lv->scroll < 0) lv->scroll = 0;

    bool changed = false;
    for (int i = 0; i < max_visible && (lv->scroll + i) < lv->count; ++i) {
        int idx = lv->scroll + i;
        int yy = y + i * line_h;
        uint32_t bg = (idx == lv->selected) ? color_sel : color_bg;
        ui_rect(x, yy, w, line_h, bg);
        ui_text(x + 4, yy + 2, lv->items[idx], color_text, bg);

        if (ev && ev->type == GUI_EVENT_MOUSE_BUTTON && (ev->mouse.buttons & 1)) {
            if (point_in(ev->mouse.x, ev->mouse.y, x, yy, w, line_h)) {
                lv->selected = idx;
                changed = true;
            }
        }
    }
    return changed;
}

void ui_progress(int x, int y, int w, int h, int percent, uint32_t fg, uint32_t bg, uint32_t border) {
    if (percent < 0) percent = 0;
    if (percent > 100) percent = 100;
    ui_rect(x, y, w, h, bg);
    int fill = (w * percent) / 100;
    if (fill > 0) ui_rect(x, y, fill, h, fg);
    // border
    ui_rect(x, y, w, 1, border);
    ui_rect(x, y + h - 1, w, 1, border);
    ui_rect(x, y, 1, h, border);
    ui_rect(x + w - 1, y, 1, h, border);
}

