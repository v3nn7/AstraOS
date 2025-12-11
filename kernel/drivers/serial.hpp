#pragma once
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void serial_init();
void serial_write(const char* s);
void serial_write_char(char c);

#ifdef __cplusplus
}
#endif
