#include "kernel.h"
#include "timer.h"

void task_sleep(uint64_t ticks) {
    uint64_t target = timer_ticks() + ticks;
    while (timer_ticks() < target) {
        scheduler_yield();
    }
}
