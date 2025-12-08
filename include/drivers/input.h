#ifndef ASTRAOS_DRIVERS_INPUT_H
#define ASTRAOS_DRIVERS_INPUT_H

#include <stdbool.h>

void input_init(void);
void keyboard_init(void);
bool keyboard_read_char(char *ch_out);
bool keyboard_poll_char(char *ch_out);

#endif /* ASTRAOS_DRIVERS_INPUT_H */

