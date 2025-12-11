#include "../include/hid_keyboard.h"
#include "../include/hid.h"

extern void input_push_key(uint8_t key, bool pressed);

static void copy_codes(uint8_t* dst, const uint8_t* src, size_t n) {
    for (size_t i = 0; i < n; ++i) {
        dst[i] = src[i];
    }
}

void hid_keyboard_reset(hid_keyboard_state_t* state) {
    if (!state) return;
    state->modifier_mask = 0;
    for (size_t i = 0; i < 6; ++i) {
        state->key_codes[i] = 0;
    }
}

uint8_t hid_keyboard_usage_to_key(uint8_t usage) {
    if (usage >= 0x04 && usage <= 0x1d) {  // 'a'..'z'
        return (uint8_t)('a' + (usage - 0x04));
    }
    if (usage >= 0x1e && usage <= 0x27) {  // '1'..'0'
        static const char nums[10] = {'1','2','3','4','5','6','7','8','9','0'};
        return (uint8_t)nums[usage - 0x1e];
    }
    switch (usage) {
        case 0x2c: return ' ';
        case 0x28: return '\n';
        case 0x2a: return '\b';
        default: return 0;
    }
}

static bool contains_code(const uint8_t* codes, size_t n, uint8_t code) {
    for (size_t i = 0; i < n; ++i) {
        if (codes[i] == code) return true;
    }
    return false;
}

void hid_keyboard_handle_report(hid_keyboard_state_t* state, const uint8_t* report, size_t len) {
    if (!state || !report || len < 8) {
        return;
    }
    uint8_t new_mod = report[0];
    const uint8_t* new_codes = &report[2];

    // Releases: keys that were present and are gone.
    for (size_t i = 0; i < 6; ++i) {
        uint8_t prev = state->key_codes[i];
        if (prev != 0 && !contains_code(new_codes, 6, prev)) {
            uint8_t ch = hid_keyboard_usage_to_key(prev);
            if (ch) {
                input_push_key(ch, false);
            }
        }
    }

    // Presses: keys newly present.
    for (size_t i = 0; i < 6; ++i) {
        uint8_t cur = new_codes[i];
        if (cur != 0 && !contains_code(state->key_codes, 6, cur)) {
            uint8_t ch = hid_keyboard_usage_to_key(cur);
            if (ch) {
                input_push_key(ch, true);
            }
        }
    }

    state->modifier_mask = new_mod;
    copy_codes(state->key_codes, new_codes, 6);
}
