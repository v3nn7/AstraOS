// Minimal heap and memory helpers for freestanding builds.
#include <stddef.h>
#include <stdint.h>
#include "klog.h"
#include <drivers/input/input_core.h>

#include "io.hpp"
#include "vmm.h"

/* ------------------------------------------------------------------------- */
/*                    Buddy + Slab allocator (C++)                           */
/* ------------------------------------------------------------------------- */

namespace {
    struct Block {
        Block* next;
        uint32_t order; /* total size = 2^order including header */
        bool free;
    };

    static constexpr size_t HEAP_SIZE   = 512 * 1024; /* 512 KiB */
    static constexpr uint32_t MIN_ORDER = 4;          /* 16 bytes */
    static constexpr uint32_t MAX_ORDER = 19;         /* 512 KiB */

    static uint8_t heap_area[HEAP_SIZE];
    static Block* free_lists[MAX_ORDER - MIN_ORDER + 1] = { nullptr };
    static bool heap_inited = false;

    inline size_t order_size(uint32_t order) { return static_cast<size_t>(1) << order; }

    void push(uint32_t order, Block* node) {
        node->next = free_lists[order - MIN_ORDER];
        node->free = true;
        node->order = order;
        free_lists[order - MIN_ORDER] = node;
    }

    Block* pop(uint32_t order) {
        Block* head = free_lists[order - MIN_ORDER];
        if (head) {
            free_lists[order - MIN_ORDER] = head->next;
            head->next = nullptr;
            head->free = false;
        }
        return head;
    }

    void init_heap() {
        if (heap_inited) return;
        heap_inited = true;
        Block* b = reinterpret_cast<Block*>(heap_area);
        b->next = nullptr;
        b->order = MAX_ORDER;
        b->free = true;
        push(MAX_ORDER, b);
    }

    Block* buddy_for(Block* blk) {
        uintptr_t base = reinterpret_cast<uintptr_t>(heap_area);
        uintptr_t addr = reinterpret_cast<uintptr_t>(blk);
        size_t size = order_size(blk->order);
        uintptr_t buddy_addr = ((addr - base) ^ size) + base;
        return reinterpret_cast<Block*>(buddy_addr);
    }

    void try_merge(Block* blk) {
        while (blk->order < MAX_ORDER) {
            Block* bud = buddy_for(blk);
            Block** cur = &free_lists[blk->order - MIN_ORDER];
            bool found = false;
            while (*cur) {
                if (*cur == bud && bud->free && bud->order == blk->order) {
                    *cur = bud->next;
                    found = true;
                    break;
                }
                cur = &(*cur)->next;
            }
            if (!found) break;
            if (bud < blk) blk = bud;
            blk->order += 1;
        }
        push(blk->order, blk);
    }

    Block* alloc_order(uint32_t order) {
        uint32_t cur = order;
        while (cur <= MAX_ORDER) {
            Block* blk = pop(cur);
            if (blk) {
                while (blk->order > order) {
                    blk->order -= 1;
                    size_t half = order_size(blk->order);
                    Block* bud = reinterpret_cast<Block*>(reinterpret_cast<uint8_t*>(blk) + half);
                    bud->order = blk->order;
                    bud->next = nullptr;
                    bud->free = true;
                    push(bud->order, bud);
                }
                return blk;
            }
            cur++;
        }
        return nullptr;
    }

    uint32_t pick_order(size_t size, size_t alignment) {
        if (alignment < sizeof(void*)) alignment = sizeof(void*);
        size_t need = size + sizeof(Block);
        /* round alignment to power of two */
        size_t align = alignment;
        if (align & (align - 1)) {
            size_t p = 1;
            while (p < align) p <<= 1;
            align = p;
        }
        if (need < align) need = align;
        uint32_t order = MIN_ORDER;
        while (order <= MAX_ORDER && order_size(order) < need) {
            order++;
        }
        return order;
    }
    /* ---------------- Slab on top of buddy ---------------- */
    struct Slab {
        Slab* next;
        uint16_t obj_size;
        uint16_t capacity;
        uint16_t free_count;
        uint16_t bitmap[1]; // flexible
    };

    static Slab* slab_head = nullptr;

    void slab_mark_used(Slab* slab, uint16_t idx) {
        slab->bitmap[idx / 16] |= (1u << (idx % 16));
        slab->free_count--;
    }

    void slab_mark_free(Slab* slab, uint16_t idx) {
        slab->bitmap[idx / 16] &= ~(1u << (idx % 16));
        slab->free_count++;
    }

    int slab_find_free(Slab* slab) {
        for (uint16_t i = 0; i < slab->capacity; ++i) {
            if ((slab->bitmap[i / 16] & (1u << (i % 16))) == 0) return i;
        }
        return -1;
    }

    void* slab_alloc_obj(uint16_t obj_size) {
        for (Slab* s = slab_head; s; s = s->next) {
            if (s->obj_size != obj_size) continue;
            if (s->free_count == 0) continue;
            int idx = slab_find_free(s);
            if (idx < 0) continue;
            slab_mark_used(s, (uint16_t)idx);
            uint8_t* base = reinterpret_cast<uint8_t*>(s + 1);
            return base + idx * obj_size;
        }
        /* allocate new slab page from buddy */
        size_t slab_space = sizeof(Slab) + obj_size * 32 + sizeof(uint16_t) * 2;
        uint32_t order = pick_order(slab_space, sizeof(void*));
        Block* blk = alloc_order(order);
        if (!blk) return nullptr;
        blk->free = false;
        uint8_t* mem = reinterpret_cast<uint8_t*>(blk);
        Slab* s = reinterpret_cast<Slab*>(mem);
        s->next = slab_head;
        s->obj_size = obj_size;
        s->capacity = 32;
        s->free_count = 32;
        s->bitmap[0] = s->bitmap[1] = 0;
        slab_head = s;
        slab_mark_used(s, 0);
        uint8_t* base = reinterpret_cast<uint8_t*>(s + 1);
        return base; /* first object */
    }

    Slab* slab_from_ptr(void* ptr, uint16_t* idx_out) {
        for (Slab* s = slab_head; s; s = s->next) {
            uint8_t* base = reinterpret_cast<uint8_t*>(s + 1);
            uint8_t* p = reinterpret_cast<uint8_t*>(ptr);
            size_t span = s->obj_size * s->capacity;
            if (p >= base && p < base + span) {
                size_t off = static_cast<size_t>(p - base);
                *idx_out = static_cast<uint16_t>(off / s->obj_size);
                return s;
            }
        }
        return nullptr;
    }
}

extern "C" void* kmalloc(size_t sz) {
    if (sz == 0) return nullptr;
    init_heap();
    if (sz <= 128) {
        uint16_t rounded = (sz <= 16) ? 16 : (sz <= 32) ? 32 : (sz <= 64) ? 64 : 128;
        return slab_alloc_obj(rounded);
    }
    uint32_t order = pick_order(sz, sizeof(void*));
    if (order > MAX_ORDER) return nullptr;
    Block* blk = alloc_order(order);
    if (!blk) return nullptr;
    blk->free = false;
    return reinterpret_cast<void*>(reinterpret_cast<uint8_t*>(blk) + sizeof(Block));
}

extern "C" void kfree(void* ptr) {
    if (!ptr) return;
    uint16_t idx = 0;
    Slab* s = slab_from_ptr(ptr, &idx);
    if (s) {
        slab_mark_free(s, idx);
        return;
    }
    Block* blk = reinterpret_cast<Block*>(reinterpret_cast<uint8_t*>(ptr) - sizeof(Block));
    blk->free = true;
    try_merge(blk);
}

extern "C" void* kmemalign(size_t alignment, size_t size) {
    if (size == 0) return nullptr;
    init_heap();
    uint32_t order = pick_order(size, alignment);
    if (order > MAX_ORDER) return nullptr;
    Block* blk = alloc_order(order);
    if (!blk) return nullptr;
    blk->free = false;
    return reinterpret_cast<void*>(reinterpret_cast<uint8_t*>(blk) + sizeof(Block));
}

extern "C" void* memset(void* dest, int val, size_t n) {
    uint8_t* d = reinterpret_cast<uint8_t*>(dest);
    for (size_t i = 0; i < n; ++i) {
        d[i] = static_cast<uint8_t>(val);
    }
    return dest;
}

extern "C" void klog(const char*);

// Feed keys into the input subsystem so PS/2/USB paths deliver to shell.
static input_device_t* g_input_dev = nullptr;
extern "C" void input_push_key(uint8_t key, bool pressed) {
    if (!g_input_dev) {
        g_input_dev = input_device_register(INPUT_DEVICE_KEYBOARD, "keyboard");
    }
    if (!g_input_dev) return;
    if (pressed) {
        input_key_char(g_input_dev, static_cast<char>(key));
        input_key_press(g_input_dev, key, 0);
    } else {
        input_key_release(g_input_dev, key);
    }
}

extern "C" uint64_t pmm_hhdm_offset;
/* Default to identity until HHDM is explicitly mapped. */
extern "C" uint64_t pmm_hhdm_offset = 0;

// MMIO helpers (assume HHDM direct map; adjust HHDM_BASE via build if needed).
extern "C" uint32_t mmio_read32(volatile uint32_t* addr) {
    uintptr_t base = (pmm_hhdm_offset != 0) ? pmm_hhdm_offset : 0;
    volatile uint32_t* p = reinterpret_cast<volatile uint32_t*>(base + reinterpret_cast<uintptr_t>(addr));
    return *p;
}

extern "C" uint8_t mmio_read8(volatile uint8_t* addr) {
    uintptr_t base = (pmm_hhdm_offset != 0) ? pmm_hhdm_offset : 0;
    volatile uint8_t* p = reinterpret_cast<volatile uint8_t*>(base + reinterpret_cast<uintptr_t>(addr));
    return *p;
}

extern "C" void mmio_write32(volatile uint32_t* addr, uint32_t val) {
    uintptr_t base = (pmm_hhdm_offset != 0) ? pmm_hhdm_offset : 0;
    volatile uint32_t* p = reinterpret_cast<volatile uint32_t*>(base + reinterpret_cast<uintptr_t>(addr));
    *p = val;
}

extern "C" void mmio_write64(volatile uint64_t* addr, uint64_t val) {
    uintptr_t base = (pmm_hhdm_offset != 0) ? pmm_hhdm_offset : 0;
    volatile uint64_t* p = reinterpret_cast<volatile uint64_t*>(base + reinterpret_cast<uintptr_t>(addr));
    *p = val;
}

extern "C" uint64_t mmio_read64(volatile uint64_t* addr) {
    uintptr_t base = (pmm_hhdm_offset != 0) ? pmm_hhdm_offset : 0;
    volatile uint64_t* p = reinterpret_cast<volatile uint64_t*>(base + reinterpret_cast<uintptr_t>(addr));
    return *p;
}

extern "C" bool pci_find_xhci(uintptr_t* mmio_base_out, uint8_t* bus_out, uint8_t* slot_out, uint8_t* func_out) {
#ifdef HOST_TEST
    if (mmio_base_out) *mmio_base_out = 0;
    if (bus_out) *bus_out = 0;
    if (slot_out) *slot_out = 0;
    if (func_out) *func_out = 0;
    return false;
#else
    klog_printf(KLOG_INFO, "pci: scanning for xHCI");
    auto force_bar32 = [](uint8_t bus, uint8_t dev, uint8_t func, uint32_t attrs) -> uint32_t {
        uint32_t mmio32 = 0xFEBF0000u;  // typical PC MMIO hole
        uint32_t addr0 = (1u << 31) | (static_cast<uint32_t>(bus) << 16) | (static_cast<uint32_t>(dev) << 11) |
                         (static_cast<uint32_t>(func) << 8) | 0x10;
        outl(0xCF8, addr0);
        outl(0xCFC, (mmio32 & 0xFFFFFFF0u) | attrs);
        // Clear upper BAR for 64-bit.
        uint32_t addr1 = (1u << 31) | (static_cast<uint32_t>(bus) << 16) | (static_cast<uint32_t>(dev) << 11) |
                         (static_cast<uint32_t>(func) << 8) | 0x14;
        outl(0xCF8, addr1);
        outl(0xCFC, 0);
        return mmio32 & 0xFFFFFFF0u;
    };

    auto read_config = [](uint8_t bus, uint8_t dev, uint8_t func, uint8_t off) -> uint32_t {
        uint32_t addr = (1u << 31) | (static_cast<uint32_t>(bus) << 16) | (static_cast<uint32_t>(dev) << 11) |
                        (static_cast<uint32_t>(func) << 8) | (off & 0xFC);
        outl(0xCF8, addr);
        return inl(0xCFC);
    };
    auto write_config = [](uint8_t bus, uint8_t dev, uint8_t func, uint8_t off, uint32_t val) {
        uint32_t addr = (1u << 31) | (static_cast<uint32_t>(bus) << 16) | (static_cast<uint32_t>(dev) << 11) |
                        (static_cast<uint32_t>(func) << 8) | (off & 0xFC);
        outl(0xCF8, addr);
        outl(0xCFC, val);
    };
    for (uint16_t bus = 0; bus < 256; ++bus) {
        for (uint8_t dev = 0; dev < 32; ++dev) {
            for (uint8_t func = 0; func < 8; ++func) {
                uint32_t id = read_config(bus, dev, func, 0x00);
                if (id == 0xFFFFFFFFu) {
                    if (func == 0) break;
                    continue;
                }
                uint32_t class_code = read_config(bus, dev, func, 0x08);
                uint8_t base = static_cast<uint8_t>((class_code >> 24) & 0xFF);
                uint8_t sub = static_cast<uint8_t>((class_code >> 16) & 0xFF);
                uint8_t prog = static_cast<uint8_t>((class_code >> 8) & 0xFF);
                if (base == 0x0C && sub == 0x03 && prog == 0x30) {
                    // xHCI
                    uint32_t bar0 = read_config(bus, dev, func, 0x10);
                    if ((bar0 & 0x1) != 0) {
                        continue;  // I/O space, skip
                    }
                    uint32_t bar1 = 0;
                    bool is64 = (((bar0 >> 1) & 0x3) == 0x2);
                    uint64_t base_addr = bar0 & 0xFFFFFFF0u;
                    if (is64) {
                        bar1 = read_config(bus, dev, func, 0x14);
                        uint64_t hi = static_cast<uint64_t>(bar1) << 32;
                        uint64_t lo = base_addr;
                        base_addr = lo | hi;
                    }
                    if (base_addr == 0) {
                        uint32_t attrs = bar0 & 0xF;
                        base_addr = force_bar32(bus, dev, func, attrs);
                    }
                    // Enable memory space + bus master.
                    uint32_t cmd = read_config(bus, dev, func, 0x04);
                    cmd |= 0x6;  // bit1=memory space, bit2=bus master
                    write_config(bus, dev, func, 0x04, cmd);

                    if (mmio_base_out) *mmio_base_out = static_cast<uintptr_t>(base_addr);
                    if (bus_out) *bus_out = static_cast<uint8_t>(bus);
                    if (slot_out) *slot_out = dev;
                    if (func_out) *func_out = func;
                    klog_printf(KLOG_INFO, "pci: xhci bus=%u dev=%u func=%u bar=0x%llx",
                                (unsigned)bus, (unsigned)dev, (unsigned)func, (unsigned long long)base_addr);
                    return true;
                }
                if (func == 0) {
                    uint32_t hdr = read_config(bus, dev, func, 0x0C);
                    if ((hdr & 0x00800000) == 0) {
                        break;  // not multifunction
                    }
                }
            }
        }
    }
    return false;
#endif
}

extern "C" uintptr_t virt_to_phys(const void* p) {
    uintptr_t v = reinterpret_cast<uintptr_t>(p);
    if (pmm_hhdm_offset != 0 && v >= pmm_hhdm_offset) {
        return v - pmm_hhdm_offset;
    }
    uintptr_t t = vmm_translate(v);
    return t ? t : v;
}

extern "C" int memcmp(const void* a, const void* b, size_t n) {
    const uint8_t* p = reinterpret_cast<const uint8_t*>(a);
    const uint8_t* q = reinterpret_cast<const uint8_t*>(b);
    for (size_t i = 0; i < n; ++i) {
        if (p[i] != q[i]) {
            return (p[i] < q[i]) ? -1 : 1;
        }
    }
    return 0;
}
