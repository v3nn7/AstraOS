// Local APIC helpers for SMP bring-up.
#pragma once

#include <stdint.h>
#include "../../util/io.hpp"

namespace lapic {

bool init();
void write(uint32_t reg, uint32_t val);
uint32_t read(uint32_t reg);
void send_ipi(uint8_t apic_id, uint32_t icr_low);
void timer_init(uint8_t vector, uint32_t divide, uint32_t initial);
uint32_t id();

}  // namespace lapic
