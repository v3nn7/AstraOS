#include "memory.h"
#include "kmalloc.h"
#include "dma.h"
#include "kernel.h"
#include "../../../tests/memory_tests.h"

void memory_subsystem_init(struct limine_memmap_response *memmap) {
    pmm_init(memmap);
    vmm_init();
    kmalloc_init();
    dma_init();
    memory_tests_run();
}

