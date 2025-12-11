#pragma once
#include <stdint.h>

class HPET {
public:
    static void init();
    static uint64_t counter();
    static void sleep_us(uint64_t us);
    static void sleep_ms(uint64_t ms);

    static inline uint64_t frequency() { return hpet_frequency; }
    static inline bool is_available() { return hpet_available; }

    /* Exposed for low-level helpers in hpet.cpp */
    static volatile uint64_t* hpet_base;
    static uint64_t hpet_frequency;   // Hz
    static bool hpet_available;
};
