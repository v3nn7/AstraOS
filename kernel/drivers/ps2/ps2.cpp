#include "ps2.hpp"

#include <stddef.h>
#include "../../util/io.hpp"
#include "../../util/logger.hpp"

extern "C" void input_push_key(uint8_t key, bool pressed);

namespace {

constexpr uint16_t kStatusPort = 0x64;
constexpr uint16_t kDataPort = 0x60;
constexpr uint8_t kStatusOutput = 1u << 0;
constexpr uint8_t kStatusInput = 1u << 1;

constexpr uint8_t kCmdDisableFirst = 0xAD;
constexpr uint8_t kCmdDisableSecond = 0xA7;
constexpr uint8_t kCmdEnableFirst = 0xAE;
constexpr uint8_t kCmdReadConfig = 0x20;
constexpr uint8_t kCmdWriteConfig = 0x60;

constexpr uint8_t kCfgFirstInterrupt = 1u << 0;
constexpr uint8_t kCfgSecondInterrupt = 1u << 1;
constexpr uint8_t kCfgSystemFlag = 1u << 2;
constexpr uint8_t kCfgFirstClock = 1u << 4;
constexpr uint8_t kCfgSecondClock = 1u << 5;

constexpr uint8_t kEnableScanning = 0xF4;

bool g_inited = false;

#ifdef HOST_TEST
static uint8_t g_queue[16];
static size_t g_q_head = 0;
static size_t g_q_tail = 0;
#endif

bool wait_input_clear() {
#ifndef HOST_TEST
    for (int i = 0; i < 100000; ++i) {
        if ((inb(kStatusPort) & kStatusInput) == 0) {
            return true;
        }
    }
    return false;
#else
    return true;
#endif
}

bool wait_output_full() {
#ifndef HOST_TEST
    for (int i = 0; i < 100000; ++i) {
        if (inb(kStatusPort) & kStatusOutput) {
            return true;
        }
    }
    return false;
#else
    return g_q_head != g_q_tail;
#endif
}

void write_cmd(uint8_t cmd) {
    if (!wait_input_clear()) {
        return;
    }
    outb(kStatusPort, cmd);
}

uint8_t read_data() {
#ifndef HOST_TEST
    return inb(kDataPort);
#else
    if (g_q_head == g_q_tail) {
        return 0;
    }
    uint8_t v = g_queue[g_q_head];
    g_q_head = (g_q_head + 1) % sizeof(g_queue);
    return v;
#endif
}

void write_data(uint8_t data) {
    if (!wait_input_clear()) {
        return;
    }
    outb(kDataPort, data);
}

void flush_output() {
#ifndef HOST_TEST
    while (inb(kStatusPort) & kStatusOutput) {
        (void)inb(kDataPort);
    }
#else
    g_q_head = g_q_tail = 0;
#endif
}

char map_scancode(uint8_t code) {
    switch (code) {
        case 0x02: return '1';
        case 0x03: return '2';
        case 0x04: return '3';
        case 0x05: return '4';
        case 0x06: return '5';
        case 0x07: return '6';
        case 0x08: return '7';
        case 0x09: return '8';
        case 0x0A: return '9';
        case 0x0B: return '0';
        case 0x10: return 'q';
        case 0x11: return 'w';
        case 0x12: return 'e';
        case 0x13: return 'r';
        case 0x14: return 't';
        case 0x15: return 'y';
        case 0x16: return 'u';
        case 0x17: return 'i';
        case 0x18: return 'o';
        case 0x19: return 'p';
        case 0x1E: return 'a';
        case 0x1F: return 's';
        case 0x20: return 'd';
        case 0x21: return 'f';
        case 0x22: return 'g';
        case 0x23: return 'h';
        case 0x24: return 'j';
        case 0x25: return 'k';
        case 0x26: return 'l';
        case 0x2C: return 'z';
        case 0x2D: return 'x';
        case 0x2E: return 'c';
        case 0x2F: return 'v';
        case 0x30: return 'b';
        case 0x31: return 'n';
        case 0x32: return 'm';
        case 0x39: return ' ';
        case 0x1C: return '\n';
        case 0x0E: return '\b';
        default: return 0;
    }
}

void handle_scancode(uint8_t code) {
    bool release = (code & 0x80u) != 0;
    uint8_t make = code & 0x7Fu;
    char ch = map_scancode(make);
    if (ch != 0) {
        input_push_key(static_cast<uint8_t>(ch), !release);
    }
}

void init_hw() {
#ifndef HOST_TEST
    // Disable both ports.
    write_cmd(kCmdDisableFirst);
    write_cmd(kCmdDisableSecond);

    // Flush output buffer.
    while (inb(kStatusPort) & kStatusOutput) {
        (void)inb(kDataPort);
    }

    // Read, update, and write config byte: set system flag, enable first clock, disable mouse.
    write_cmd(kCmdReadConfig);
    if (!wait_output_full()) {
        return;
    }
    uint8_t cfg = read_data();
    cfg |= kCfgSystemFlag;
    cfg &= ~kCfgSecondInterrupt;
    cfg &= ~kCfgSecondClock;
    cfg &= ~kCfgFirstInterrupt;  // keep IRQ masked until ready
    write_cmd(kCmdWriteConfig);
    write_data(cfg);

    // Enable first port clock and interrupts.
    cfg |= kCfgFirstClock;
    cfg |= kCfgFirstInterrupt;
    write_cmd(kCmdWriteConfig);
    write_data(cfg);

    // Enable scanning on the keyboard.
    write_data(kEnableScanning);

    // Enable first port.
    write_cmd(kCmdEnableFirst);
#endif
    g_inited = true;
    klog("ps2: initialized fallback keyboard");
}

}  // namespace

namespace ps2 {

void disable_legacy() {
    // Disable keyboard and mouse, flush any pending data to unblock BIOS handoff.
    write_cmd(kCmdDisableFirst);
    write_cmd(kCmdDisableSecond);
    flush_output();
}

void init() {
    disable_legacy();
    init_hw();
}

void poll() {
    if (!g_inited) {
        return;
    }
    // Drain all pending bytes.
    while (wait_output_full()) {
        uint8_t code = read_data();
        handle_scancode(code);
#ifndef HOST_TEST
        // Continue draining real hardware only while buffer has data.
        if ((inb(kStatusPort) & kStatusOutput) == 0) {
            break;
        }
#else
        // On host, stop when queue empty.
        if (g_q_head == g_q_tail) {
            break;
        }
#endif
    }
}

#ifdef HOST_TEST
void debug_inject_scancode(uint8_t code) {
    size_t next = (g_q_tail + 1) % sizeof(g_queue);
    if (next == g_q_head) {
        return;  // drop if full
    }
    g_queue[g_q_tail] = code;
    g_q_tail = next;
}
#endif

}  // namespace ps2
