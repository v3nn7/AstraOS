#pragma once

#include "types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* APIC helpers (stubs for build compatibility) */
static inline void apic_send_eoi(uint8_t vector) { (void)vector; }
static inline uint8_t apic_alloc_vector(void) { return 0; }

#ifdef __cplusplus
}
#endif
