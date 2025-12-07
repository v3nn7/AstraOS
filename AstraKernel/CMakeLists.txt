cmake_minimum_required(VERSION 3.20)

# Cross-compiler settings BEFORE project()
set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR x86_64)
set(CMAKE_C_COMPILER /opt/elf/bin/x86_64-elf-gcc)
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

project(AstraOS C CXX ASM_NASM)

enable_language(ASM_NASM)
set(CMAKE_POSITION_INDEPENDENT_CODE OFF)

set(CMAKE_ASM_NASM_OBJECT_FORMAT elf64)
set(CMAKE_ASM_NASM_FLAGS "-Wall")

set(CFLAGS_KERNEL
    -std=gnu99 -ffreestanding -O2 -Wall -Wextra
    -fno-stack-protector -fno-omit-frame-pointer
    -fno-pic -fno-pie
    -mno-red-zone -mcmodel=kernel
    -mno-sse -mno-sse2 -mno-mmx -mgeneral-regs-only -msoft-float
    -nostdlib -nostdinc -fno-builtin -m64
    -Ikernel/include -Iboot
)

set(CXXFLAGS_KERNEL
    -std=c++17 -ffreestanding -O2 -Wall -Wextra
    -fno-exceptions -fno-rtti -fno-stack-protector -fno-omit-frame-pointer
    -fno-pic -fno-pie
    -mno-red-zone -mcmodel=kernel
    -mno-sse -mno-sse2 -mno-mmx -mgeneral-regs-only -msoft-float
    -nostdlib -nostdinc -fno-builtin -m64
    -Ikernel/include -Iboot -Ikernel/arch/x86_64/mm
)

set(LDFLAGS_KERNEL "-nostdlib -z max-page-size=0x1000 -no-pie -T ${CMAKE_SOURCE_DIR}/linker.ld")

file(GLOB_RECURSE KERNEL_C
    "kernel/*.c"
    "kernel/*/*.c"
    "kernel/*/*/*.c"
    "boot/limine_requests.c"
)
# Add C++ sources explicitly (no globs from C list)
file(GLOB_RECURSE KERNEL_CPP
    "kernel/arch/x86_64/mm/*.cpp"
    "kernel/core/*.cpp"
    "kernel/drivers/usb/hid/parser/*.cpp"
    "kernel/drivers/usb/hid/hid_driver.cpp"
)
# Ensure new memory subsystem entry point is always compiled
list(APPEND KERNEL_C
    "kernel/arch/x86_64/mm/memory.c"
    "kernel/arch/x86_64/drivers/load_cursor.c"
    "kernel/drivers/usb/hid/hid_usb_driver.c"
    "kernel/drivers/usb/host/pci_usb_routing.c"
    "kernel/drivers/usb/xhci/xhci_init_complete.c"
)

# Newly added core/subsystems
list(APPEND KERNEL_C
    "kernel/core/klog.c"
    "kernel/core/shell.c"
    "kernel/dev/driver_manager.c"
    "kernel/dev/tty.c"
    "kernel/fs/devfs.c"
    "kernel/fs/ramfs.c"
    "kernel/fs/initrd.c"
    "kernel/ipc/ipc.c"
    "kernel/tests/kernel_tests.c"
)
set(KERNEL_ASM boot/kernel_entry.asm)

# D memset disabled (SSE instructions causing early faults); use C version
set(MEMSET_SRC "")

# Lodepng for PNG decoding (cursor images)
file(GLOB_RECURSE LODEPNG_CPP "lodepng/lodepng/lodepng.cpp")
list(APPEND LODEPNG_CPP 
    "kernel/arch/x86_64/drivers/lodepng_alloc.cpp"
    "kernel/arch/x86_64/drivers/lodepng_wrapper.cpp"
)
set(LODEPNG_SRC ${LODEPNG_CPP})

add_executable(kernel.elf ${KERNEL_C} ${KERNEL_CPP} ${KERNEL_ASM} ${MEMSET_SRC} ${LODEPNG_SRC})
set_target_properties(kernel.elf PROPERTIES LINK_FLAGS "${LDFLAGS_KERNEL}")
target_compile_options(kernel.elf PRIVATE $<$<COMPILE_LANGUAGE:C>:${CFLAGS_KERNEL}>)
target_compile_options(kernel.elf PRIVATE $<$<COMPILE_LANGUAGE:CXX>:${CXXFLAGS_KERNEL}>)
# Disable C++ wrapper and disk I/O in lodepng to avoid stdlib dependencies
# Also need to provide limits.h stub and redirect malloc/free/realloc
set_source_files_properties(${LODEPNG_SRC} PROPERTIES COMPILE_FLAGS "-DLODEPNG_NO_COMPILE_CPP -DLODEPNG_NO_COMPILE_DISK -DLODEPNG_NO_COMPILE_ALLOCATORS -Ikernel/include")

target_include_directories(kernel.elf PRIVATE
    ${CMAKE_SOURCE_DIR}/kernel/include
    ${CMAKE_SOURCE_DIR}/kernel/gui
    ${CMAKE_SOURCE_DIR}/kernel/installer
    ${CMAKE_SOURCE_DIR}/kernel/tests
    ${CMAKE_SOURCE_DIR}/boot
    ${CMAKE_SOURCE_DIR}/lodepng/lodepng
    ${CMAKE_SOURCE_DIR}/assets
)

# Limine 10.4.0 (pre-downloaded)
# After cloning and building, binaries are in bin/ subdirectory
set(LIMINE_BIN_DIR ${CMAKE_BINARY_DIR}/_deps/limine-src/bin)

set(OVMF_CODE_CAND
    "/usr/share/ovmf/x64/OVMF_CODE.fd"
    "/usr/share/ovmf/x64/OVMF_CODE.4m.fd"
    "/usr/share/OVMF/OVMF_CODE.fd"
    "/usr/share/OVMF/OVMF_CODE.4m.fd"
    "/usr/share/edk2-ovmf/x64/OVMF_CODE.fd"
    "/usr/share/edk2-ovmf/x64/OVMF_CODE.4m.fd"
)
set(OVMF_VARS_CAND
    "/usr/share/ovmf/x64/OVMF_VARS.fd"
    "/usr/share/ovmf/x64/OVMF_VARS.4m.fd"
    "/usr/share/OVMF/OVMF_VARS.fd"
    "/usr/share/OVMF/OVMF_VARS.4m.fd"
    "/usr/share/edk2-ovmf/x64/OVMF_VARS.fd"
    "/usr/share/edk2-ovmf/x64/OVMF_VARS.4m.fd"
)

foreach(c ${OVMF_CODE_CAND})
    if(EXISTS ${c})
        set(OVMF_CODE ${c})
        break()
    endif()
endforeach()

foreach(c ${OVMF_VARS_CAND})
    if(EXISTS ${c})
        set(OVMF_VARS_SRC ${c})
        break()
    endif()
endforeach()

if(OVMF_CODE AND OVMF_VARS_SRC)
    set(OVMF_VARS "${CMAKE_BINARY_DIR}/OVMF_VARS.fd")
    add_custom_command(OUTPUT ${OVMF_VARS}
        COMMAND ${CMAKE_COMMAND} -E copy ${OVMF_VARS_SRC} ${OVMF_VARS}
        DEPENDS ${OVMF_VARS_SRC}
        COMMENT "Copying OVMF_VARS.fd to build dir"
    )
    add_custom_target(ovmf_vars ALL DEPENDS ${OVMF_VARS})
else()
    message(WARNING "OVMF not found; run_bios will be available, run_uefi will print a message.")
endif()

# ISO BUILD TARGET
add_custom_target(iso ALL
    DEPENDS kernel.elf
    COMMAND ${CMAKE_COMMAND} -E rm -rf ${CMAKE_BINARY_DIR}/iso_root
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/iso_root/boot
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/iso_root/EFI/BOOT

    # Copy kernel
    COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:kernel.elf> ${CMAKE_BINARY_DIR}/iso_root/boot/kernel.elf

    # Generate initrd with assets
    COMMAND python3 ${CMAKE_SOURCE_DIR}/tools/generate_initrd.py
        --output ${CMAKE_BINARY_DIR}/iso_root/boot/initrd.img
        --assets-dir ${CMAKE_SOURCE_DIR}/assets

    # Copy Limine config (Limine 10.x uses limine.conf)
    COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_SOURCE_DIR}/boot/limine.conf ${CMAKE_BINARY_DIR}/iso_root/boot/limine.conf
    COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_SOURCE_DIR}/boot/limine.conf ${CMAKE_BINARY_DIR}/iso_root/limine.conf

    # Copy Limine BIOS files
    COMMAND ${CMAKE_COMMAND} -E copy ${LIMINE_BIN_DIR}/limine-bios.sys ${CMAKE_BINARY_DIR}/iso_root/boot/limine-bios.sys
    COMMAND ${CMAKE_COMMAND} -E copy ${LIMINE_BIN_DIR}/limine-bios-cd.bin ${CMAKE_BINARY_DIR}/iso_root/boot/limine-bios-cd.bin

    # Copy UEFI boot files
    COMMAND ${CMAKE_COMMAND} -E copy ${LIMINE_BIN_DIR}/limine-uefi-cd.bin ${CMAKE_BINARY_DIR}/iso_root/boot/limine-uefi-cd.bin
    COMMAND ${CMAKE_COMMAND} -E copy ${LIMINE_BIN_DIR}/BOOTX64.EFI ${CMAKE_BINARY_DIR}/iso_root/EFI/BOOT/BOOTX64.EFI

    # Build ISO with proper BIOS and UEFI El Torito entries
    COMMAND xorriso -as mkisofs
        -b boot/limine-bios-cd.bin
        -no-emul-boot -boot-load-size 4 -boot-info-table
        --efi-boot boot/limine-uefi-cd.bin
        -efi-boot-part --efi-boot-image --protective-msdos-label
        ${CMAKE_BINARY_DIR}/iso_root
        -o ${CMAKE_BINARY_DIR}/AstraOS.iso
)

add_custom_target(run_bios
    COMMAND qemu-system-x86_64
        -machine q35
        -vga virtio
        -display sdl
        -m 4096M
        -serial stdio
        -no-reboot -no-shutdown
        -cdrom ${CMAKE_BINARY_DIR}/AstraOS.iso
        -boot d
        -device qemu-xhci,id=xhci
        -device usb-kbd,bus=xhci.0
        -device usb-mouse,bus=xhci.0
        -nodefaults
    DEPENDS iso
)

if(OVMF_CODE AND OVMF_VARS_SRC)
    add_custom_target(run_uefi
        COMMAND qemu-system-x86_64
            -machine q35
            -cpu qemu64
            -m 2048M
            -device qemu-xhci,id=xhci
            -device usb-kbd,bus=xhci.0
            -device usb-mouse,bus=xhci.0
            -device usb-tablet,bus=xhci.0
            -vga virtio
            -display sdl
            -drive if=pflash,format=raw,readonly=on,file=${OVMF_CODE}
            -drive if=pflash,format=raw,file=${OVMF_VARS}
            -drive format=raw,file=${CMAKE_BINARY_DIR}/AstraOS.iso
            -serial stdio
            -no-reboot -no-shutdown
            -nodefaults
        DEPENDS iso ovmf_vars
    )
else()
    add_custom_target(run_uefi
        COMMAND ${CMAKE_COMMAND} -E echo "OVMF not found; install edk2-ovmf to enable run_uefi."
        DEPENDS iso
    )
endif()
