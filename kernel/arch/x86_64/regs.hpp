// Basic register/stack frame definitions for x86_64 interrupts.
#pragma once
#include <stdint.h>

struct InterruptFrame {
    uint64_t rip;
    uint16_t cs;
    uint16_t padding_cs;
    uint32_t padding_cs2;
    uint64_t rflags;
    uint64_t rsp;
    uint16_t ss;
    uint16_t padding_ss;
    uint32_t padding_ss2;
};
