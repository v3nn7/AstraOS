#pragma once

#include "types.h"
#include "event.h"

typedef struct {
    int x, y, w, h;
    const char *text;
} ui_button_t;

typedef struct {
    const char **items;
    int count;
    int selected;
    int scroll;
    int visible;
} ui_listview_t;

/* Drawing helpers */
void ui_clear(uint32_t color_bg);
void ui_rect(int x, int y, int w, int h, uint32_t color);
void ui_text(int x, int y, const char *txt, uint32_t fg, uint32_t bg);

/* Widgets (return true on click/selection change) */
bool ui_button(ui_button_t *btn, gui_event_t *ev, uint32_t color, uint32_t color_text);
bool ui_listview(ui_listview_t *lv, gui_event_t *ev, int x, int y, int w, int h,
                 uint32_t color_bg, uint32_t color_sel, uint32_t color_text);

/* Progress bar */
void ui_progress(int x, int y, int w, int h, int percent, uint32_t fg, uint32_t bg, uint32_t border);

