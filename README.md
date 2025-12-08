# AstraOS 0.0.2 skeleton

Folder roles:
- `include/` — public headers for kernel, arch, drivers, memory, and utils
- `kernel/` — core entry points like `kmain` and panic handling
- `arch/x86_64/` — architecture-specific boot, CPU bring-up, and timer code
- `mm/` — memory manager in C++ (PMM, VMM, heap, paging, regions)
- `drivers/` — device drivers; `drivers/usb/*` reserved for imported USB stack
- `lib/` — basic libc-style helpers (string, printf, utils)
- `build/` — build artifacts output placeholder
- `tests/` — unit tests stubs for public API coverage

Notes:
- Boot flow uses a Multiboot2 header and a stub jump to `kmain`.
- No Limine dependency; boot code is generic Multiboot2-ready.

