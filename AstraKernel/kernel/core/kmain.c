#include "types.h"
#include "kernel.h"
#include "memory.h"
#include "interrupts.h"
#include "drivers.h"
#include "limine.h"
#include "vmm.h"
#include "pmm.h"
#include "klog.h"
#include "timer.h"
#include "initcall.h"
#include "driver_manager.h"
#include "tty.h"
#include "vfs.h"
#include "panic.h"
/* Embedded cursor (optional, compile with -DCURSOR_EMBEDDED) */
#ifdef CURSOR_EMBEDDED
#include "cursor.h"
#endif

extern volatile struct limine_framebuffer_request limine_fb_request;
extern volatile struct limine_memmap_request limine_memmap_request;
extern uint64_t initial_stack_top;

/* ---------------------------------------------------------
   Framebuffer init
--------------------------------------------------------- */

static bool init_framebuffer(void) {
    printf("FB: checking response...\n");

    struct limine_framebuffer_response *resp = limine_fb_request.response;
    if (!resp || resp->framebuffer_count == 0) {
        printf("FB: no framebuffer available\n");
        return false;
    }

    struct limine_framebuffer *fb = resp->framebuffers[0];

    if (fb->pitch == 0 || fb->width == 0 || fb->height == 0) {
        printf("FB: invalid pitch/size\n");
        return false;
    }

    if (fb->bpp != 24 && fb->bpp != 32) {
        printf("FB: unsupported bpp=%u (only 24/32 supported)\n", fb->bpp);
        return false;
    }

    uint64_t fb_addr = (uint64_t)fb->address;   // virtual (HHDM)
    uint64_t fb_phys = vmm_virt_to_phys(fb_addr);

    printf(
        "FB: virt=%p phys=%p w=%u h=%u pitch=%u bpp=%u\n",
        fb->address,
        (void*)fb_phys,
        fb->width,
        fb->height,
        fb->pitch,
        fb->bpp
    );

    fb_init(
        fb_addr,
        fb->width,
        fb->height,
        fb->pitch,
        fb->bpp
    );
    
    /* Verify framebuffer is accessible by writing a test pattern */
    uint32_t *test_fb = (uint32_t *)fb_addr;
    printf("FB: writing test pattern to %p\n", (void *)fb_addr);
    for (int i = 0; i < 100; i++) {
        test_fb[i] = 0xFF00FF00; /* Green test pixels */
    }
    printf("FB: test pattern written\n");

    return true;
}


/* ---------------------------------------------------------
                   K M A I N
--------------------------------------------------------- */

void kmain(void) {
    klog_init();
    klog_printf(KLOG_INFO, "kmain: start");

    /* CPU / MMU Init */
    gdt_init((uint64_t)&initial_stack_top);
    idt_init();
    memory_subsystem_init(limine_memmap_request.response);
    irq_init();

    /* Framebuffer */
    if (!init_framebuffer()) {
        vga_init();
        vga_write("Framebuffer unavailable. VGA fallback active.\n");
        for (;;) __asm__("hlt");
    }

    /* Clear framebuffer using fb_clear (safer) */
    printf("kmain: clearing framebuffer\n");
    fb_clear(0x000000); /* Black background */
    printf("kmain: framebuffer cleared\n");
    
    /* Test pixel */
    struct limine_framebuffer *fb = limine_fb_request.response->framebuffers[0];
    ((uint32_t*)fb->address)[0] = 0xFF0000; /* Red pixel */
    printf("kmain: test pixel written\n");

    timer_init(100);
    klog_printf(KLOG_INFO, "kmain: timer ready");
    printf("kmain: timer ready, starting initcalls\n");

    /* Initcalls */
    printf("kmain: calling initcall_run_all()\n");
    initcall_run_all();
    printf("kmain: initcalls done\n");
    klog_printf(KLOG_INFO, "kmain: initcalls done");

    /* Filesystems */
    printf("kmain: mounting devfs\n");
    devfs_mount();
    printf("kmain: mounting ramfs\n");
    ramfs_mount();
    printf("kmain: vfs mounted\n");
    klog_printf(KLOG_INFO, "kmain: vfs mounted");
    
    /* Load initrd (CPIO archive with assets) */
    {
        extern void initrd_load(void);
        initrd_load();
    }
    
    /* Setup assets directory for cursor and other resources */
    cursor_setup_assets();

    /* Drivers */
    printf("kmain: initializing keyboard\n");
    keyboard_init();
    printf("kmain: initializing mouse\n");
    mouse_init();
    
    /* Load cursor image from file system */
    /* Try multiple locations in order of preference */
    const char *cursor_paths[] = {
        "/assets/cursor.png",
        "/usr/share/cursor.png",
        "/etc/cursor.png",
        "/cursor.png",
        NULL
    };
    
    int cursor_loaded = 0;
    for (int i = 0; cursor_paths[i] != NULL; i++) {
        printf("kmain: trying to load cursor from %s\n", cursor_paths[i]);
        int result = mouse_cursor_load_from_file(cursor_paths[i]);
        if (result == 0) {
            unsigned w = 0, h = 0;
            mouse_cursor_get_size(&w, &h);
            printf("kmain: cursor loaded successfully from %s (%ux%u)\n", cursor_paths[i], w, h);
            cursor_loaded = 1;
            break;
        } else {
            printf("kmain: failed to load cursor from %s (error=%d)\n", cursor_paths[i], result);
        }
    }
    
    /* Fallback: try embedded cursor if file loading failed */
    if (!cursor_loaded) {
        #ifdef CURSOR_EMBEDDED
        extern const unsigned char cursor_png[];
        extern const unsigned int cursor_png_len;
        printf("kmain: trying embedded cursor (%u bytes)\n", cursor_png_len);
        int result = mouse_cursor_load_from_memory(cursor_png, cursor_png_len);
        if (result == 0) {
            unsigned w = 0, h = 0;
            mouse_cursor_get_size(&w, &h);
            printf("kmain: embedded cursor loaded successfully (%ux%u)\n", w, h);
            cursor_loaded = 1;
        }
        #endif
        
        if (!cursor_loaded) {
            printf("kmain: no cursor image found, using fallback\n");
        }
    }
    printf("kmain: initializing USB\n");
    usb_init();
    printf("kmain: initializing HID\n");
    hid_init();
    printf("kmain: initializing TTY\n");
    tty_init();
    printf("kmain: drivers initialized\n");
    klog_printf(KLOG_INFO, "kmain: drivers initialized");

    printf("kmain: enabling interrupts\n");
    interrupts_enable();
    printf("kmain: interrupts enabled\n");
    klog_printf(KLOG_INFO, "kmain: interrupts enabled");

    printf("kmain: initializing scheduler\n");
    scheduler_init();
    printf("kmain: scheduler initialized\n");
    klog_printf(KLOG_INFO, "kmain: scheduler init");

    printf("AstraKernel started\n");
    klog_printf(KLOG_INFO, "kmain: launching shell");
    printf("kmain: launching shell\n");

    shell_run();

    printf("kmain: shell returned (should not happen)\n");
    klog_printf(KLOG_INFO, "kmain: shell returned");

    for (;;) {
        __asm__ volatile("hlt");
    }
}
