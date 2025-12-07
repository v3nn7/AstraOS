cmake_minimum_required(VERSION 3.20)

set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR x86_64)
set(CMAKE_C_COMPILER /opt/elf/bin/x86_64-elf-gcc)
set(CMAKE_CXX_COMPILER /opt/elf/bin/x86_64-elf-g++)
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

project(AstraOS C CXX ASM_NASM)

enable_language(ASM_NASM)
set(CMAKE_ASM_NASM_OBJECT_FORMAT elf64)

set(CFLAGS_KERNEL
    -std=gnu99 -ffreestanding -O2 -Wall -Wextra
    -fno-stack-protector -fno-omit-frame-pointer
    -fno-pic -fno-pie
    -mno-red-zone -mcmodel=kernel
    -nostdlib -nostdinc -fno-builtin -m64
)

set(CXXFLAGS_KERNEL
    -std=c++20 -ffreestanding -O2 -Wall -Wextra
    -fno-exceptions -fno-rtti -fno-stack-protector
    -fno-omit-frame-pointer -fno-pic -fno-pie
    -mno-red-zone -mcmodel=kernel
    -nostdlib -nostdinc -fno-builtin -m64
)

set(LDFLAGS_KERNEL "-nostdlib -z max-page-size=0x1000 -no-pie -T ${CMAKE_SOURCE_DIR}/linker.ld")

file(GLOB_RECURSE KERNEL_C "kernel/**/*.c")
file(GLOB_RECURSE KERNEL_CPP "kernel/**/*.cpp")
set(KERNEL_ASM boot/kernel_entry.asm)
set(BOOT_C boot/limine_requests.c)
set(LODEPNG_CPP lodepng/lodepng/lodepng.cpp)

add_executable(kernel.elf ${KERNEL_C} ${KERNEL_CPP} ${KERNEL_ASM} ${BOOT_C} ${LODEPNG_CPP})
set_target_properties(kernel.elf PROPERTIES LINK_FLAGS "${LDFLAGS_KERNEL}")

target_compile_options(kernel.elf PRIVATE $<$<COMPILE_LANGUAGE:C>:${CFLAGS_KERNEL}>)
target_compile_options(kernel.elf PRIVATE $<$<COMPILE_LANGUAGE:CXX>:${CXXFLAGS_KERNEL}>)

# Disable FPU/SSE/MMX for interrupt handlers in idt.c, irq.c, and isr.c
set_source_files_properties(kernel/arch/x86_64/cpu/idt.c PROPERTIES
    COMPILE_FLAGS "-mno-80387 -mno-sse -mno-mmx"
)
set_source_files_properties(kernel/arch/x86_64/interrupts/irq.c PROPERTIES
    COMPILE_FLAGS "-mno-80387 -mno-sse -mno-mmx"
)
set_source_files_properties(kernel/arch/x86_64/interrupts/isr.c PROPERTIES
    COMPILE_FLAGS "-mno-80387 -mno-sse -mno-mmx"
)

# Disable C++ features and disk/stdlib features in lodepng (kernel environment)
set_source_files_properties(lodepng/lodepng/lodepng.cpp PROPERTIES
    COMPILE_FLAGS "-DLODEPNG_NO_COMPILE_CPP -DLODEPNG_NO_COMPILE_DISK -DLODEPNG_NO_COMPILE_ALLOCATORS"
)

target_include_directories(kernel.elf PRIVATE kernel/include boot kernel/installer kernel/gui)

# ------------------------------------
# LIMINE 10.4.0-binary (your version)
# ------------------------------------

set(LIMINE_DIR "${CMAKE_SOURCE_DIR}/limine")

foreach(f
    "BOOTX64.EFI"
    "limine-bios.sys"
    "limine-bios-cd.bin"
    "limine-uefi-cd.bin"
)
    if(NOT EXISTS "${LIMINE_DIR}/${f}")
        message(FATAL_ERROR "Missing Limine file: ${LIMINE_DIR}/${f}")
    endif()
endforeach()

# ---------------------------
# LIMINE TOOL BUILD
# ---------------------------

set(LIMINE_TOOL "${LIMINE_DIR}/limine")
add_custom_command(
    OUTPUT ${LIMINE_TOOL}
    COMMAND ${CMAKE_COMMAND} -E chdir ${LIMINE_DIR} ${CMAKE_MAKE_PROGRAM}
    COMMENT "Building limine tool"
)

# ---------------------------
# ISO BUILD
# ---------------------------

set(ISO_ROOT "${CMAKE_BINARY_DIR}/iso_root")
set(ISO_FILE "${CMAKE_BINARY_DIR}/AstraOS.iso")

add_custom_target(iso ALL
    DEPENDS kernel.elf ${LIMINE_TOOL}
    COMMAND ${CMAKE_COMMAND} -E rm -rf ${ISO_ROOT}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${ISO_ROOT}/boot
    COMMAND ${CMAKE_COMMAND} -E make_directory ${ISO_ROOT}/EFI/BOOT

    COMMAND ${CMAKE_COMMAND} -E copy kernel.elf ${ISO_ROOT}/boot/kernel.elf
    COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_SOURCE_DIR}/boot/limine.conf ${ISO_ROOT}/boot/limine.conf
    COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_SOURCE_DIR}/boot/limine.conf ${ISO_ROOT}/limine.conf

    COMMAND python3 ${CMAKE_SOURCE_DIR}/tools/generate_initrd.py
            --output ${ISO_ROOT}/boot/initrd.img
            --assets-dir ${CMAKE_SOURCE_DIR}/assets

    COMMAND ${CMAKE_COMMAND} -E copy ${LIMINE_DIR}/BOOTX64.EFI ${ISO_ROOT}/EFI/BOOT/BOOTX64.EFI
    COMMAND ${CMAKE_COMMAND} -E copy ${LIMINE_DIR}/limine-uefi-cd.bin ${ISO_ROOT}/boot/limine-uefi-cd.bin
    COMMAND ${CMAKE_COMMAND} -E copy ${LIMINE_DIR}/limine-bios-cd.bin ${ISO_ROOT}/boot/limine-bios-cd.bin
    COMMAND ${CMAKE_COMMAND} -E copy ${LIMINE_DIR}/limine-bios.sys ${ISO_ROOT}/boot/limine-bios.sys

    COMMAND xorriso -as mkisofs
        -b boot/limine-bios-cd.bin
        -no-emul-boot -boot-load-size 4 -boot-info-table
        --efi-boot boot/limine-uefi-cd.bin
        -efi-boot-part --efi-boot-image
        ${ISO_ROOT} -o ${ISO_FILE}

    COMMAND ${LIMINE_DIR}/limine bios-install ${ISO_FILE}
)

# ---------------------------
# QEMU RUN TARGETS
# ---------------------------

add_custom_target(run_bios
    COMMAND qemu-system-x86_64
        -machine q35 -m 4096
        -vga virtio
        -cdrom ${ISO_FILE}
        -serial stdio
        -boot d
    DEPENDS iso
)

add_custom_target(run_uefi
    COMMAND qemu-system-x86_64
        -machine q35 -m 4096
        -bios /usr/share/edk2-ovmf/x64/OVMF.4m.fd
        -vga virtio
        -cdrom ${ISO_FILE}
        -serial stdio
        -boot d
    DEPENDS iso
)
