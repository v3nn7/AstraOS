CXX              := clang++
HOST_CXX         := clang++
LINKER           := lld-link
RUSTC            := rustc
RUST_TARGET      ?=
QEMU             ?= qemu-system-x86_64
OVMF_CODE        ?= /home/v3nn7/OVMF/ovmf-code-x86_64.fd

TARGET_NAME      := kernel.efi
BUILD_DIR        := build
OBJ_DIR          := $(BUILD_DIR)/obj
KERNEL_BIN_DIR   := $(BUILD_DIR)/kernel
KERNEL_BIN       := $(KERNEL_BIN_DIR)/$(TARGET_NAME)
ISO_DIR          := $(BUILD_DIR)/iso
BOOTEFI          ?= /usr/lib/systemd/boot/efi/systemd-bootx64.efi
EFI_PART_LABEL   ?= AstraOS
TEST_BIN         := $(BUILD_DIR)/tests/kernel_test
ESP_DIR          := $(BUILD_DIR)/esp
ESP_IMG          := $(ESP_DIR)/esp.img
BOOT_SRC         := boot
BOOT_LOADER_CONF := $(BOOT_SRC)/loader.conf
BOOT_ENTRY_CONF  := $(BOOT_SRC)/entries/astra.conf

CXXFLAGS := --target=x86_64-pc-win32-coff -ffreestanding -fno-exceptions -fno-rtti \
            -fshort-wchar -mno-red-zone -fno-stack-protector -Wall -Wextra -Werror \
            -fdata-sections -ffunction-sections -std=c++17 -Ikernel

LDFLAGS := /machine:x64 /subsystem:efi_application /entry:efi_main /nodefaultlib /align:4096

SRC_DIR := kernel
ASM_SOURCES := $(SRC_DIR)/arch/x86_64/isr_stubs.S \
               $(SRC_DIR)/arch/x86_64/gdt_asm.S \
               $(SRC_DIR)/arch/x86_64/idt_asm.S
CPP_SOURCES := $(SRC_DIR)/main.cpp \
               $(SRC_DIR)/arch/x86_64/gdt.cpp \
               $(SRC_DIR)/arch/x86_64/idt.cpp \
               $(SRC_DIR)/arch/x86_64/lapic.cpp \
               $(SRC_DIR)/arch/x86_64/smp.cpp \
               $(SRC_DIR)/ui/renderer.cpp \
               $(SRC_DIR)/ui/shell.cpp \
               $(SRC_DIR)/util/logger.cpp \
               $(SRC_DIR)/util/memory.cpp \
               $(SRC_DIR)/drivers/serial.cpp \
               $(SRC_DIR)/drivers/usb/usb_core.cpp \
               $(SRC_DIR)/drivers/usb/xhci.cpp \
               $(SRC_DIR)/drivers/usb/usb_hid.cpp \
               $(SRC_DIR)/drivers/usb/usb_keyboard.cpp
RUST_OBJS := $(if $(RUST_TARGET),$(OBJ_DIR)/crypto/crypto.o,)
RUST_SOURCES := crypto/mod.rs crypto/sha256.rs crypto/rng.rs
RUSTFLAGS := -O -C panic=abort $(if $(RUST_TARGET),--target $(RUST_TARGET),)
OBJS := $(patsubst $(SRC_DIR)/%.cpp,$(OBJ_DIR)/%.o,$(CPP_SOURCES)) \
        $(patsubst $(SRC_DIR)/%.S,$(OBJ_DIR)/%.o,$(ASM_SOURCES)) \
        $(RUST_OBJS)
HOST_CXXFLAGS := -std=c++17 -Wall -Wextra -Werror -DHOST_TEST

.PHONY: all kernel iso run test clean distclean

all: kernel

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(OBJ_DIR):
	mkdir -p $(OBJ_DIR)

$(KERNEL_BIN_DIR):
	mkdir -p $(KERNEL_BIN_DIR)

$(ESP_DIR):
	mkdir -p $(ESP_DIR)

$(BUILD_DIR)/tests: | $(BUILD_DIR)
	mkdir -p $(BUILD_DIR)/tests

$(ESP_IMG): $(KERNEL_BIN) $(BOOT_LOADER_CONF) $(BOOT_ENTRY_CONF) | $(ESP_DIR)
	truncate -s 8M $(ESP_IMG)
	mkfs.vfat -n ASTRAESP $(ESP_IMG)
	mmd -i $(ESP_IMG) ::/EFI ::/EFI/BOOT ::/EFI/AstraOS ::/loader ::/loader/entries
	mcopy -i $(ESP_IMG) $(BOOTEFI) ::/EFI/BOOT/BOOTX64.EFI
	mcopy -i $(ESP_IMG) $(KERNEL_BIN) ::/EFI/AstraOS/kernel.efi
	mcopy -i $(ESP_IMG) $(BOOT_LOADER_CONF) ::/loader/loader.conf
	mcopy -i $(ESP_IMG) $(BOOT_ENTRY_CONF) ::/loader/entries/astra.conf

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp | $(OBJ_DIR)
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.S | $(OBJ_DIR)
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(OBJ_DIR)/crypto/crypto.o: $(RUST_SOURCES) | $(OBJ_DIR)
	@mkdir -p $(dir $@)
	$(RUSTC) $(RUSTFLAGS) --emit=obj -o $@ crypto/mod.rs

$(KERNEL_BIN): $(OBJS) linker.ld | $(KERNEL_BIN_DIR)
	$(LINKER) $(OBJS) $(LDFLAGS) /out:$@

kernel: $(KERNEL_BIN)

$(TEST_BIN): tests/kernel_test.cpp $(SRC_DIR)/main.cpp | $(BUILD_DIR)/tests
	$(HOST_CXX) $(HOST_CXXFLAGS) tests/kernel_test.cpp -o $@

test: $(TEST_BIN)
	$(TEST_BIN)

iso: kernel $(ESP_IMG)
	@if [ ! -f "$(BOOTEFI)" ]; then \
		echo "Missing BOOTX64.EFI (set BOOTEFI=/path/to/systemd-bootx64.efi)"; \
		exit 1; \
	fi
	mkdir -p $(ISO_DIR)/EFI/BOOT $(ISO_DIR)/loader/entries
	cp $(BOOTEFI) $(ISO_DIR)/EFI/BOOT/BOOTX64.EFI
	mkdir -p $(ISO_DIR)/EFI/AstraOS
	cp $(KERNEL_BIN) $(ISO_DIR)/EFI/BOOT/kernel.efi
	cp $(KERNEL_BIN) $(ISO_DIR)/EFI/AstraOS/kernel.efi
	cp $(BOOT_LOADER_CONF) $(ISO_DIR)/loader/loader.conf
	cp $(BOOT_ENTRY_CONF) $(ISO_DIR)/loader/entries/astra.conf
	xorriso -as mkisofs -R -J -l -iso-level 3 -V "$(EFI_PART_LABEL)" \
		-append_partition 2 0xef $(ESP_IMG) \
		-e --interval:appended_partition_2:all:: -no-emul-boot \
		-isohybrid-gpt-basdat \
		-o AstraOS.iso $(ISO_DIR)

run: iso
	$(QEMU) -machine q35 -m 2G \
		-bios $(OVMF_CODE) \
		-drive if=none,id=cdrom,file=AstraOS.iso,format=raw,media=cdrom \
		-device ide-cd,drive=cdrom \
		-device qemu-xhci,id=xhci \
		-device usb-kbd,bus=xhci.0 \
		-boot d

clean:
	rm -f kernel.efi
	rm -rf $(BUILD_DIR)

distclean: clean
	rm -f AstraOS.iso
