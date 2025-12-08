#include <kernel/kmain.h>
#include <boot/multiboot2.h>
#include <drivers/framebuffer.h>
#include <klog.h>
#include <stdint.h>

static void parse_multiboot_fb(uint32_t mb_info_addr) {
    mb2_info_t *info = (mb2_info_t *)(uintptr_t)mb_info_addr;
    uint8_t *ptr = (uint8_t *)info + sizeof(mb2_info_t);
    uint8_t *end = (uint8_t *)info + info->total_size;

    while (ptr < end) {
        mb2_tag_t *tag = (mb2_tag_t *)ptr;
        if (tag->type == MB2_TAG_TYPE_END || tag->size == 0) break;

        if (tag->type == MB2_TAG_TYPE_FRAMEBUFFER) {
            mb2_tag_framebuffer_t *fb = (mb2_tag_framebuffer_t *)tag;
            framebuffer_init(fb->framebuffer_addr, fb->framebuffer_width, fb->framebuffer_height, fb->framebuffer_pitch, fb->framebuffer_bpp);
            klog_printf(KLOG_INFO, "fb: %ux%u pitch=%u bpp=%u addr=0x%llx",
                        fb->framebuffer_width, fb->framebuffer_height, fb->framebuffer_pitch, fb->framebuffer_bpp,
                        (unsigned long long)fb->framebuffer_addr);
            framebuffer_clear(0x00FF0000); /* czerwone tło dla widoczności */
            return;
        }

        ptr += (tag->size + 7) & ~7u; /* 8-byte align */
    }

    klog_printf(KLOG_WARN, "fb: no framebuffer tag found");
}

void kmain(uint32_t mb_info) {
    parse_multiboot_fb(mb_info);
    for (;;);
}

