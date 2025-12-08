#include <kmalloc.h>
#include <klog.h>
#include <stddef.h>

void* operator new(unsigned long size) {
    return kmalloc(size);
}

void operator delete(void* ptr) noexcept {
    kfree(ptr);
}

void operator delete(void* ptr, unsigned long) noexcept {
    kfree(ptr);
}

void* operator new[](unsigned long size) {
    return kmalloc(size);
}

void operator delete[](void* ptr) noexcept {
    kfree(ptr);
}

void operator delete[](void* ptr, unsigned long) noexcept {
    kfree(ptr);
}

