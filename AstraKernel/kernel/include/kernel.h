#pragma once

#include "types.h"
#include "interrupts.h"

#ifdef __cplusplus
extern "C" {
#endif

void scheduler_init(void);
int scheduler_add_task(void (*entry)(void *), void *arg);
void scheduler_tick(interrupt_frame_t *frame);
void scheduler_yield(void);
bool scheduler_kill(int pid);

int create_process(void (*entry)(void *), void *arg);
int kill_process(int pid);

void task_sleep(uint64_t ticks);

int printf(const char *fmt, ...);

void kmain(void);
void shell_run(void);

#ifdef __cplusplus
}
#endif
