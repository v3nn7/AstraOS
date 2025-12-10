// Basic SMP bring-up scaffolding.
#pragma once

#include <stdint.h>

namespace smp {

bool init();
uint32_t core_count();
uint32_t bsp_apic_id();

}  // namespace smp
