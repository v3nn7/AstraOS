# Changelog

All notable changes to this project will be documented in this file.


## 0.0.1.minor - 2025-12-11
- Relocated USB and input headers under `kernel/include/drivers/...` and refreshed all driver includes to use `<drivers/...>` paths that match the new layout.
- Updated the build to track the current USB/input sources and added `kernel/include` to the compiler search paths to keep the reorganized headers discoverable.
- Adjusted USB structure docs to reflect the new include root.
- Wired the new paging/VMM/PMM sources into the build; vmm-backed `virt_to_phys` now falls back to translation when HHDM is absent.
- USB PCI detection now maps controller BARs through `vmm_map_mmio` for consistent uncached MMIO access.
- Added x86_64 CR0/CR3/CR4 assembly helpers (`paging_asm.S`) and linked them so the new VMM can enable paging without undefined symbols.
- Brought PCI MSI/MSI-X helpers into `drivers/PCI/` with stubbed allocators and setup paths, updating tests and the build to consume them.
- Added xHCI structure header `xhci_structs.h` and wired xHCI stubs to honor its TRB/ring layouts.
- xHCI PCI probe now reads BARs, enables bus mastering, sets up MSI/legacy IRQ via `pci_setup_interrupt`, allocates a controller instance, and registers it with the USB host core.
- Stubbed core USB stack pieces (device, transfer, HID, debug/helpers) so host tests can enumerate a fake HID keyboard/mouse and exercise control/interrupt flows.

## 0.0.1.1exp - 2025-12-10
- Bumped version metadata to experimental iteration 0.0.1.1exp.
- Added `renderer_rect_outline` for drawing bordered rectangles with coverage checks and host test coverage.
- Shell now supports keyboard input with backspace and enter-driven commits.
- Shell handles `help` and `clear` commands with simple history responses.
- Shell draws input cursor and persists a scrolling history log in the window frame.
- Added xHCI USB core scaffolding with TRB ring setup and command stubs.
- Added generic HID parsing helpers and a keyboard driver mapping usages to keycodes.
- Interrupt-IN endpoint wiring for keyboards with event dispatch into `input_push_key`.
- Host tests covering keyboard report handling and HID mapping.
- HHDM-aware `virt_to_phys` to ensure xHCI sees physical addresses; xHCI init now logs MMIO base and configured HHDM offset for troubleshooting.

## 0.0.1 - 2025-12-10
- Initial UEFI framebuffer bootstrap for AstraOS using systemd-boot.
- Minimal C++ kernel that draws "AstraOS 0.0.1 booted successfully!" via GOP.
- Build tooling: PE/COFF linker script and Makefile targets for kernel, ISO, and QEMU run.
- Fixed kernel build by removing unused text helpers referencing an undefined font and stubbing serial I/O for host tests.
