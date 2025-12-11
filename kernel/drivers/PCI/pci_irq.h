#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

int pci_configure_irq(uint8_t bus, uint8_t slot, uint8_t func, uint8_t legacy_irq, uint8_t *vector_out);

#ifdef __cplusplus
}
#endif
