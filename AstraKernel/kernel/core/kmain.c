#include "types.h"
#include "kernel.h"
#include "memory.h"
#include "interrupts.h"
#include "drivers.h"
#include "limine.h"

extern volatile struct limine_framebuffer_request limine_fb_request;
extern volatile struct limine_memmap_request limine_memmap_request;
extern char _kernel_start_physical[];
extern char _kernel_end_physical[];
extern uint64_t initial_stack_top;

static inline uint64_t align_up_u64(uint64_t v, uint64_t a) { return (v + a - 1) & ~(a - 1); }
static inline uint64_t align_down_u64(uint64_t v, uint64_t a) { return v & ~(a - 1); }

static void map_framebuffer_region(struct limine_framebuffer *fb) {
    if (!fb) return;
    uint64_t fb_phys = (uint64_t)fb->address;
    uint64_t fb_size = (uint64_t)fb->pitch * fb->height;
    uint64_t start = align_down_u64(fb_phys, PAGE_SIZE);
    uint64_t end   = align_up_u64(fb_phys + fb_size, PAGE_SIZE);
    for (uint64_t p = start; p < end; p += PAGE_SIZE) {
        map_page((virt_addr_t)p, (phys_addr_t)p, PAGE_WRITE | PAGE_CACHE_DISABLE);
    }
}

static void init_framebuffer(void) {
    if (!limine_fb_request.response) return;
    struct limine_framebuffer_response *resp = (struct limine_framebuffer_response *)limine_fb_request.response;
    if (resp->framebuffer_count == 0) return;
    struct limine_framebuffer *fb = resp->framebuffers[0];
    map_framebuffer_region(fb);
    fb_init((uint64_t)fb->address, fb->width, fb->height, fb->pitch, fb->bpp);
}

void kmain(void) {
    gdt_init((uint64_t)&initial_stack_top);
    idt_init();
    irq_init();
    paging_init((phys_addr_t)_kernel_start_physical, (phys_addr_t)_kernel_end_physical);

    init_framebuffer();
    keyboard_init();
    interrupts_enable();

    printf("AstraKernel (Limine) start\n");

    scheduler_init();
    shell_run();

    for (;;) {
        __asm__ volatile("hlt");
    }
}
