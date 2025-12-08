cmake_minimum_required(VERSION 3.20)

project(AstraOS VERSION 0.0.2 LANGUAGES C CXX ASM)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ============================================================
# GLOBAL CONFIG
# ============================================================
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Wyłącz standardową bibliotekę (bo OS nie ma libc)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -ffreestanding -fno-stack-protector -fno-pic -mno-red-zone -Wall -Wextra")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -ffreestanding -fno-exceptions -fno-rtti -fno-stack-protector -fno-pic -mno-red-zone -Wall -Wextra")

# LINKER FLAGS
set(KERNEL_LINK_FLAGS
    -T
    ${CMAKE_SOURCE_DIR}/linker.ld
    -nostdlib
    -no-pie
    -z
    max-page-size=0x1000
)

# ============================================================
# GLOBAL INCLUDE
# ============================================================
include_directories(
    ${CMAKE_SOURCE_DIR}/include
)

# ============================================================
# KERNEL TARGET
# ============================================================
add_executable(kernel.elf
    arch/x86_64/boot/boot.s

    kernel/kmain.c
    kernel/panic.c

    arch/x86_64/cpu/gdt.c
    arch/x86_64/cpu/idt.c
    arch/x86_64/cpu/isr.c
    arch/x86_64/cpu/cpu_init.c

    arch/x86_64/timer/pit.c

    drivers/input/input_init.c
    drivers/pci/pci.c
    drivers/framebuffer/fb.c

    # USB + HID =================================================
    drivers/usb/core/usb_core.c
    drivers/usb/core/usb_device.c
    drivers/usb/core/usb_transfer.c
    drivers/usb/core/usb_descriptors.c
    drivers/usb/core/usb_enumeration.c
    drivers/usb/core/usb_manager.c
    drivers/usb/core/usb_init.c
    drivers/usb/hub/usb_hub.c
    drivers/usb/host/ehci_qh.c
    drivers/usb/host/ehci_td.c
    drivers/usb/host/ehci.c
    drivers/usb/host/ohci.c
    drivers/usb/host/pci_msi.c
    drivers/usb/host/pci_usb_detect.c
    drivers/usb/host/pci_usb_routing.c
    drivers/usb/host/uhci.c
    drivers/usb/host/xhci_irq.c
    drivers/usb/host/xhci.c
    drivers/usb/hid/hid_keyboard.c
    drivers/usb/hid/hid_mouse.c
    drivers/usb/hid/hid_usb_driver.c
    drivers/usb/hid/hid_driver.cpp
    drivers/usb/hid/parser/hid_parser.cpp
    drivers/usb/hid/parser/hid_report_descriptor.cpp
    drivers/usb/util/usb_debug.c
    drivers/usb/util/usb_helpers.c
    drivers/usb/xhci/xhci_ring.c
    drivers/usb/xhci/xhci_transfer.c
    drivers/usb/xhci/xhci_context.c
    drivers/usb/xhci/xhci_device.c
    drivers/usb/xhci/xhci_doorbell.c
    drivers/usb/xhci/xhci_init_complete.c
    drivers/usb/xhci/xhci_debug.c
    # ===========================================================

    lib/string.c
    lib/kmalloc.c
    lib/klog.c
    lib/apic_stub.c
    lib/event_stub.c
    lib/timer_stub.c
    lib/pmm_stub.c
    lib/printf.c
    lib/utils.c
    lib/input_stub.c
    lib/mouse_cursor_stub.c
    lib/irq_stub.c
    lib/new.cpp

    # C++
    mm/pmm.cpp
    mm/vmm.cpp
    mm/heap.cpp
    mm/region.cpp
    mm/paging.cpp
)

# ============================================================
# LINKING
# ============================================================
target_link_options(kernel.elf PRIVATE ${KERNEL_LINK_FLAGS})

# ============================================================
# TESTS
# ============================================================
# Tests are disabled in this stub build
