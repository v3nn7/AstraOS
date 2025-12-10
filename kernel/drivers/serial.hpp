#pragma once
#include <stdint.h>

void serial_init();
void serial_write(const char* s);
void serial_write_char(char c);
