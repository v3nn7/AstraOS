#pragma once

#include "types.h"

/* TSS management functions */
void tss_init(uint64_t kernel_stack_top);
void tss_set_rsp0(uint64_t rsp);
uint64_t tss_get_rsp0(void);
void tss_set_ist(uint8_t index, uint64_t rsp);
void tss_update_from_current_rsp(void);

