#include "types.h"
#include "drivers.h"

#define VGA_MEM   ((volatile uint16_t *)0xB8000)
#define VGA_COLS 80
#define VGA_ROWS 25

static uint8_t vga_color = 0x07; /* light grey on black */
static uint16_t vga_row = 0;
static uint16_t vga_col = 0;

static inline uint16_t vga_entry(char c, uint8_t color) {
    return ((uint16_t)color << 8) | (uint8_t)c;
}

void vga_clear(void) {
    for (uint16_t y = 0; y < VGA_ROWS; ++y) {
        for (uint16_t x = 0; x < VGA_COLS; ++x) {
            VGA_MEM[y * VGA_COLS + x] = vga_entry(' ', vga_color);
        }
    }
    vga_row = 0;
    vga_col = 0;
}

static void vga_scroll(void) {
    if (vga_row < VGA_ROWS)
        return;
    /* Move lines up */
    for (uint16_t y = 1; y < VGA_ROWS; ++y) {
        for (uint16_t x = 0; x < VGA_COLS; ++x) {
            VGA_MEM[(y - 1) * VGA_COLS + x] = VGA_MEM[y * VGA_COLS + x];
        }
    }
    /* Clear last line */
    for (uint16_t x = 0; x < VGA_COLS; ++x) {
        VGA_MEM[(VGA_ROWS - 1) * VGA_COLS + x] = vga_entry(' ', vga_color);
    }
    vga_row = VGA_ROWS - 1;
}

void vga_putc(char c) {
    if (c == '\n') {
        vga_col = 0;
        vga_row++;
        vga_scroll();
        return;
    } else if (c == '\r') {
        vga_col = 0;
        return;
    } else if (c == '\b') {
        if (vga_col > 0) {
            vga_col--;
            VGA_MEM[vga_row * VGA_COLS + vga_col] = vga_entry(' ', vga_color);
        }
        return;
    }

    VGA_MEM[vga_row * VGA_COLS + vga_col] = vga_entry(c, vga_color);
    vga_col++;
    if (vga_col >= VGA_COLS) {
        vga_col = 0;
        vga_row++;
        vga_scroll();
    }
}

void vga_write(const char *s) {
    while (s && *s) {
        vga_putc(*s++);
    }
}

void vga_init(void) {
    vga_color = 0x07;
    vga_clear();
}
