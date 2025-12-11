#pragma once
#include "types.h"

#ifdef __cplusplus
extern "C" {
#endif

int ahci_init(void);
int ahci_read_lba(uint64_t lba, void *buf, size_t sectors);

#ifdef __cplusplus
}
#endif

