// Simple shell UI helpers for AstraOS experimental builds.
#pragma once
#include <stddef.h>
#include <stdint.h>

#include "renderer.hpp"

struct ShellConfig {
    uint32_t win_x;
    uint32_t win_y;
    uint32_t win_w;
    uint32_t win_h;
    Rgb background;
    Rgb window;
    Rgb title_bar;
    Rgb foreground;
    Rgb accent;
};

// Initialize shell state and draw the initial UI.
void shell_init(const ShellConfig& cfg);

// Handle a single key (printable ASCII, backspace, or enter).
void shell_handle_key(char key);

// Render the full shell window using current state.
void shell_render();

#ifdef HOST_TEST
// Debug helpers for host-side unit tests.
const char* shell_debug_input();
size_t shell_debug_input_len();
size_t shell_debug_history_size();
const char* shell_debug_history_line(size_t idx);
#endif
