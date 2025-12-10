# Changelog

All notable changes to this project will be documented in this file.

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

## 0.0.1 - 2025-12-10
- Initial UEFI framebuffer bootstrap for AstraOS using systemd-boot.
- Minimal C++ kernel that draws "AstraOS 0.0.1 booted successfully!" via GOP.
- Build tooling: PE/COFF linker script and Makefile targets for kernel, ISO, and QEMU run.
- Fixed kernel build by removing unused text helpers referencing an undefined font and stubbing serial I/O for host tests.
