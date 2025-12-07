#include "memory.h"
#include "kmalloc.h"
#include "dma.h"
#include "kernel.h"

void memory_subsystem_init(struct limine_memmap_response *memmap) {
    printf("memory: starting PMM init\n");
    pmm_init(memmap);
    printf("memory: PMM done, starting VMM init\n");
    vmm_init();
    printf("memory: VMM done, starting kmalloc init\n");
    kmalloc_init();
    printf("memory: kmalloc done, starting DMA init\n");
    dma_init();
    printf("memory: all subsystems initialized\n");
}

