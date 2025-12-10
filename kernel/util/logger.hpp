#pragma once
#include <stdint.h>
#include "../ui/renderer.hpp"

extern "C" {
void logger_init();
void klog(const char* msg);
[[noreturn]] void kpanic(const char* msg);
}
