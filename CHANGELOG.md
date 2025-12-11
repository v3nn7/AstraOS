# Changelog

All notable changes to this project will be documented in this file.


0.0.2.PATCH — 2025-12-11

-PMM: Complete rewrite with proper multi-region support. Each UEFI region (type 7) has its own bitmap. Regions are sorted by phys_start. alloc_page() returns HHDM-mapped virtual addresses. alloc_contiguous() finds contiguous blocks within single regions with alignment and max_phys support. Fixed DMA allocation failures by ensuring all pages come from the same physical region.
-DMA: Simplified allocator using pmm_alloc_contiguous_dma() directly. All allocations are <4GB and properly aligned (min 64 bytes). Returns HHDM virtual addresses with physical addresses in phys_out parameter.
-xHCI: Added full implementations for Enable Slot, Address Device, and Configure Endpoint commands on the   -Command Ring. Implemented setup of Input Context, Device Context, and Endpoint Contexts (DCBAA, EP0, EP1). -Doorbell handling added for command submission. Event Ring (ERDP/IMAN, cycle state) now processes events -correctly, including re-queuing interrupt TRBs for EP1 (keyboard). Transfer Events now parse HID reports and -automatically resubmit the TRB.
-xHCI: Expanded MMIO mapping to 0x10000. Main loop now polls the controller when needed. Normal TRB for EP1 (8 -bytes) is armed programmatically and doorbelled using DCI=2.
-HID: Boot-protocol keyboard parser integrated with Transfer Events; the IRQ buffer is automatically reissued -after each report.
-Build/Run: Simplified QEMU configuration (kernel-irqchip=split, removed intel-iommu). Version bumped to 0.0.2.-PATCH.
-ACPI: Added `request_hub_osc` helper and wired ACPI_OSC_HUB.cpp/hpp into the build with host-test coverage.
-Core: Guarded USB polling when no host controllers are detected and log SMP init failures instead of assuming success.
-Licensing: Added ASAL v2.1 header to key public headers (`interrupts.h`, `ahci.h`, `types.h`).
-Build: `make run` now depends on `make iso`, so the ISO is generated before launching QEMU.
-DMA: Added ASAL v2.1 header, exported paging_map_dma, re-used the single virt_to_phys implementation, provided phys_to_virt, and fixed C build issues (NULL vs nullptr).
-PMM: Removed unused bitmap counter to silence -Wunused warnings during build.

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
- Filled previously empty USB modules with functional helpers: status/error/memory/pipe/string handling, enumeration/power/init/manager shims, HID glue, PCI USB detection, EHCI QH/TD allocators, OHCI init, hub stubs, USB monitor/tree dumps, and xHCI caps/context/scheduler helpers (all logging-enabled).
- Added PCI config/IRQ helpers (`pci_config.[ch]`, `pci_irq.[ch]`): ECAM or CF8/CFC access, BAR decoding, busmaster enable, IRQ setup wrapper; updated xHCI and PCI scan to use shared API. 
- xHCI bring-up expanded: port power/reset and status change events, software command completions for Enable Slot/Address/Configure EP, transfer rings with doorbells for control/interrupt/bulk, event ring processing, and enumeration now drives driver binding (HID) after SET_CONFIGURATION.
- input_push_key teraz rejestruje klawiaturę w input_core i kolejkuje znaki/klawisze; pętla główna odpytuje input_core i podaje je do shell_handle_key, więc PS/2/USB trafia do konsoli.

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
