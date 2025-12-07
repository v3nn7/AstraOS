#pragma once

#include "types.h"

void tty_init(void);
void tty_putc(char c);
void tty_write(const char *s);
bool tty_read_char(char *out);
void tty_feed_char(char c);
void tty_poll_input(void);

