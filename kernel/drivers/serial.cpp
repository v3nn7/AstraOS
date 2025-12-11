#include "serial.hpp"

#include "../util/io.hpp"

namespace {
constexpr uint16_t COM1 = 0x3F8;

bool serial_ready = false;

void serial_write_reg(uint16_t reg, uint8_t val) {
    outb(COM1 + reg, val);
}

uint8_t serial_read_reg(uint16_t reg) {
    return inb(COM1 + reg);
}

bool tx_empty() {
    return serial_read_reg(5) & 0x20;
}

}  // namespace

extern "C" {

void serial_init() {
#ifdef HOST_TEST
    serial_ready = false;
    return;
#endif
    serial_write_reg(1, 0x00);  // disable interrupts
    serial_write_reg(3, 0x80);  // enable DLAB
    serial_write_reg(0, 0x03);  // divisor low (38400 baud)
    serial_write_reg(1, 0x00);  // divisor high
    serial_write_reg(3, 0x03);  // 8 bits, no parity, one stop
    serial_write_reg(2, 0xC7);  // enable FIFO
    serial_write_reg(4, 0x0B);  // IRQs enabled, RTS/DSR set
    serial_ready = true;
}

void serial_write_char(char c) {
    if (!serial_ready) {
        return;
    }
    while (!tx_empty()) {
    }
    outb(COM1, static_cast<uint8_t>(c));
}

void serial_write(const char* s) {
    if (!serial_ready) {
        return;
    }
    for (size_t i = 0; s[i] != 0; ++i) {
        serial_write_char(s[i]);
    }
}

}  // extern "C"
