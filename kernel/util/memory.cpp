// Minimal heap and memory helpers for freestanding builds.
#include <stddef.h>
#include <stdint.h>

#include "io.hpp"

extern "C" void* kmalloc(size_t sz) {
    static uint8_t heap[64 * 1024];
    static size_t offset = 0;
    if (offset + sz > sizeof(heap)) {
        return nullptr;
    }
    void* ptr = heap + offset;
    offset += sz;
    return ptr;
}

extern "C" void kfree(void*) {
    // Simple bump allocator: no free.
}

extern "C" void* kmemalign(size_t alignment, size_t size) {
    // Very simple aligned bump allocator.
    static uint8_t heap[64 * 1024];
    static size_t offset = 0;
    size_t aligned = (offset + alignment - 1) & ~(alignment - 1);
    if (aligned + size > sizeof(heap)) {
        return nullptr;
    }
    void* ptr = heap + aligned;
    offset = aligned + size;
    return ptr;
}

extern "C" void* memset(void* dest, int val, size_t n) {
    uint8_t* d = reinterpret_cast<uint8_t*>(dest);
    for (size_t i = 0; i < n; ++i) {
        d[i] = static_cast<uint8_t>(val);
    }
    return dest;
}

// Stub to satisfy input subsystem; platform code should override.
extern "C" void input_push_key(uint8_t, bool) {}

// MMIO helpers (trivial identity mapping stubs; platform should provide real mapping).
extern "C" uint32_t mmio_read32(uintptr_t addr) {
    volatile uint32_t* p = reinterpret_cast<volatile uint32_t*>(addr);
    return *p;
}

extern "C" uint8_t mmio_read8(uintptr_t addr) {
    volatile uint8_t* p = reinterpret_cast<volatile uint8_t*>(addr);
    return *p;
}

extern "C" void mmio_write32(uintptr_t addr, uint32_t val) {
    volatile uint32_t* p = reinterpret_cast<volatile uint32_t*>(addr);
    *p = val;
}

extern "C" bool pci_find_xhci(uintptr_t* mmio_base_out, uint8_t* bus_out, uint8_t* slot_out, uint8_t* func_out) {
#ifdef HOST_TEST
    if (mmio_base_out) *mmio_base_out = 0;
    if (bus_out) *bus_out = 0;
    if (slot_out) *slot_out = 0;
    if (func_out) *func_out = 0;
    return false;
#else
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
                    uint64_t base_addr = bar0 & 0xFFFFFFF0u;
                    // If 64-bit BAR, read high dword
                    if (((bar0 >> 1) & 0x3) == 0x2) {
                        uint32_t bar1 = read_config(bus, dev, func, 0x14);
                        base_addr |= (static_cast<uint64_t>(bar1) << 32);
                    }
                    // Enable memory space + bus master.
                    uint32_t cmd = read_config(bus, dev, func, 0x04);
                    cmd |= 0x6;  // bit1=memory space, bit2=bus master
                    write_config(bus, dev, func, 0x04, cmd);

                    if (mmio_base_out) *mmio_base_out = static_cast<uintptr_t>(base_addr);
                    if (bus_out) *bus_out = static_cast<uint8_t>(bus);
                    if (slot_out) *slot_out = dev;
                    if (func_out) *func_out = func;
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
    // Identity mapping in stub; platform must provide real translation.
    return reinterpret_cast<uintptr_t>(p);
}
