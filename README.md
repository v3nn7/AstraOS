## AstraOS

Minimal x86_64 UEFI kernel with framebuffer shell, USB/HID scaffolding, and WIP xHCI/AHCI bring-up.

### Build prerequisites (Ubuntu)
- `clang`, `lld`, `make`
- `mtools`, `xorriso`, `systemd-boot-efi`

### Building
- `make iso` — builds `AstraOS.iso` and EFI binary at `build/kernel/kernel.efi`
- `make run` — builds and boots QEMU (requires OVMF at `~/OVMF/ovmf-code-x86_64.fd`; adjust in `Makefile` if different)
- `make test` — host-side unit tests (stubs, no hardware access)

### Running in QEMU
```
make run
```
Default QEMU args (from `Makefile`): `-machine q35 -m 4G -bios ~/OVMF/ovmf-code-x86_64.fd -device qemu-xhci -device usb-kbd`.

### Debugging
- Serial log is written to `.cursor/debug.log` when using `make run`.
- Kernel logger: see `kernel/util/klog.c`; logs emitted via `klog_printf`.

### Status
- PMM/VMM present; ACPI stubs replaced with basic parsers.
- LAPIC/IOAPIC/MSI work in progress.
- USB stack: stubs plus HID parser; xHCI rings/commands under development.
- AHCI scaffold present; DMA path pending.
