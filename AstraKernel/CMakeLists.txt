cmake_minimum_required(VERSION 3.20)
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)
project(AstraOS C ASM_NASM)

set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_C_COMPILER /opt/elf/bin/x86_64-elf-gcc)
set(CMAKE_ASM_NASM_OBJECT_FORMAT elf64)
set(CMAKE_ASM_NASM_FLAGS "-Wall")

enable_language(C)
enable_language(ASM_NASM)

set(CFLAGS_KERNEL
    -std=gnu99 -ffreestanding -O2 -Wall -Wextra -Wno-unused-parameter
    -fno-stack-protector -fno-omit-frame-pointer -mno-red-zone -mcmodel=kernel
    -mno-sse -mno-sse2 -mno-mmx -mgeneral-regs-only -msoft-float
    -nostdlib -nostdinc -fno-builtin -m64
    -Ikernel/include -Iboot
)
set(LDFLAGS_KERNEL "-nostdlib -z max-page-size=0x1000 -T ${CMAKE_SOURCE_DIR}/linker.ld")

file(GLOB_RECURSE KERNEL_C "kernel/*.c" "boot/limine_requests.c")
set(KERNEL_ASM boot/kernel_entry.asm)
set(D_SOURCE ${CMAKE_SOURCE_DIR}/kernel/lib/memset.d)

find_program(LDC2_EXECUTABLE ldc2)
if(NOT LDC2_EXECUTABLE)
    message(FATAL_ERROR "ldc2 not found; install ldc2 to build D object")
endif()

set(D_OBJ ${CMAKE_BINARY_DIR}/memset.o)
add_custom_command(OUTPUT ${D_OBJ}
    COMMAND ${LDC2_EXECUTABLE} -betterC -O2 -release -c ${D_SOURCE} -of=${D_OBJ}
    DEPENDS ${D_SOURCE}
    COMMENT "Compiling D memset with ldc2"
)
add_custom_target(d_objs DEPENDS ${D_OBJ})

add_executable(kernel.elf ${KERNEL_C} ${KERNEL_ASM} ${D_OBJ})
add_dependencies(kernel.elf d_objs)
set_target_properties(kernel.elf PROPERTIES LINK_FLAGS "${LDFLAGS_KERNEL}")
target_compile_options(kernel.elf PRIVATE $<$<COMPILE_LANGUAGE:C>:${CFLAGS_KERNEL}>)
target_include_directories(kernel.elf PRIVATE ${CMAKE_SOURCE_DIR}/kernel/include ${CMAKE_SOURCE_DIR}/boot)

include(FetchContent)
FetchContent_Declare(limine
    GIT_REPOSITORY https://github.com/limine-bootloader/limine.git
    GIT_TAG v7.x-binary
)
FetchContent_MakeAvailable(limine)
set(LIMINE_BIN_DIR ${limine_SOURCE_DIR})

add_custom_target(iso ALL
    DEPENDS kernel.elf
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/iso/boot
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/iso/EFI/BOOT
    COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:kernel.elf> ${CMAKE_BINARY_DIR}/iso/boot/kernel.elf
    COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_SOURCE_DIR}/boot/limine.cfg ${CMAKE_BINARY_DIR}/iso/limine.cfg
    COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_SOURCE_DIR}/boot/limine.cfg ${CMAKE_BINARY_DIR}/iso/boot/limine.cfg
    COMMAND ${CMAKE_COMMAND} -E copy ${LIMINE_BIN_DIR}/limine-bios.sys ${CMAKE_BINARY_DIR}/iso/limine-bios.sys
    COMMAND ${CMAKE_COMMAND} -E copy ${LIMINE_BIN_DIR}/limine-bios-cd.bin ${CMAKE_BINARY_DIR}/iso/limine-bios-cd.bin
    COMMAND ${CMAKE_COMMAND} -E copy ${LIMINE_BIN_DIR}/limine-uefi-cd.bin ${CMAKE_BINARY_DIR}/iso/limine-uefi-cd.bin
    COMMAND ${CMAKE_COMMAND} -E copy ${LIMINE_BIN_DIR}/BOOTX64.EFI ${CMAKE_BINARY_DIR}/iso/EFI/BOOT/BOOTX64.EFI
    COMMAND xorriso -as mkisofs -b limine-bios-cd.bin -no-emul-boot -boot-load-size 4 -boot-info-table --efi-boot limine-uefi-cd.bin -efi-boot-part --efi-boot-image --protective-msdos-label ${CMAKE_BINARY_DIR}/iso -o ${CMAKE_BINARY_DIR}/AstraOS.iso
    COMMENT "Building AstraOS ISO via Limine"
)

add_custom_target(run
    COMMAND qemu-system-x86_64 -m 512M -serial stdio -no-reboot -no-shutdown -cdrom ${CMAKE_BINARY_DIR}/AstraOS.iso -boot d
    DEPENDS iso
)
