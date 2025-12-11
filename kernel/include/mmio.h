#pragma once

#include "types.h"

#ifdef __cplusplus
extern "C" {
#endif

uint32_t mmio_read32(volatile uint32_t *addr);
uint8_t mmio_read8(volatile uint8_t *addr);
void mmio_write32(volatile uint32_t *addr, uint32_t val);
void mmio_write64(volatile uint64_t *addr, uint64_t val);
uint64_t mmio_read64(volatile uint64_t *addr);

#ifdef __cplusplus
}
#endif
