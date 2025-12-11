/**
 * C++ LAPIC shim wrapping the C LAPIC driver.
 */
#pragma once

#include <stdint.h>
#include <drivers/apic/lapic.h>

namespace lapic {

inline bool init() {
    lapic_init();
    return true;
}

inline uint32_t id() {
    return lapic_read(LAPIC_ID) >> 24;
}

inline void send_ipi(uint8_t apic_id, uint32_t flags) {
    lapic_send_ipi(apic_id, flags);
}

}  // namespace lapic
