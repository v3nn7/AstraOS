#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#define HAS_PLACEMENT_NEW
#include <string.h>

#define HOST_TEST 1
#ifdef HOST_TEST
// Stub serial I/O to satisfy logger without touching hardware in host tests.
void serial_init() {}
void serial_write(const char*) {}
void serial_write_char(char) {}
#endif

// Kernel allocation, logging, and hw stubs for host tests.
extern "C" void* kmalloc(size_t sz) { return malloc(sz); }
extern "C" void kfree(void* p) { free(p); }
extern "C" void* kmemalign(size_t alignment, size_t size) {
    void* p = nullptr;
    posix_memalign(&p, alignment, size);
    return p;
}
extern "C" uintptr_t virt_to_phys(const void* p) { return reinterpret_cast<uintptr_t>(p); }
extern "C" uint32_t mmio_read32(uintptr_t) { return 0; }
extern "C" uint8_t mmio_read8(uintptr_t) { return 0; }
extern "C" void mmio_write32(uintptr_t, uint32_t) {}
extern "C" bool pci_find_xhci(uintptr_t* base, uint8_t* b, uint8_t* d, uint8_t* f) {
    if (base) *base = 0;
    if (b) *b = 0;
    if (d) *d = 0;
    if (f) *f = 0;
    return false;
}

// Input subsystem capture for keyboard events.
static uint8_t gKeys[16];
static bool gPressed[16];
static size_t gKeyEvents = 0;
extern "C" void input_push_key(uint8_t key, bool pressed) {
    if (gKeyEvents < 16) {
        gKeys[gKeyEvents] = key;
        gPressed[gKeyEvents] = pressed;
    }
    ++gKeyEvents;
}
#include "../kernel/ui/renderer.cpp"
#include "../kernel/ui/shell.cpp"
#include "../kernel/drivers/usb/core/usb_core.c"
#include "../kernel/drivers/usb/core/usb_request.c"
#include "../kernel/drivers/usb/core/usb_transfer.c"
#include "../kernel/drivers/usb/core/usb_device.c"
#include "../kernel/drivers/usb/usb_core.cpp"
#include "../kernel/drivers/usb/msi_allocator.c"
#include "../kernel/drivers/usb/msi.c"
#include "../kernel/drivers/usb/msix.c"
#include "../kernel/drivers/usb/xhci/xhci_init.c"
#include "../kernel/drivers/usb/xhci/xhci_ring.c"
#include "../kernel/drivers/usb/xhci/xhci_events.c"
#include "../kernel/drivers/usb/xhci/xhci_commands.c"
#include "../kernel/drivers/usb/xhci/xhci_transfer.c"
#include "../kernel/drivers/usb/hid/hid.c"
#include "../kernel/drivers/usb/hid/hid_keyboard.c"
#include "../kernel/drivers/usb/hid/hid_mouse.c"
#include "../kernel/drivers/usb/hid/hid_parser.c"
#include "../kernel/drivers/ps2/ps2.cpp"
#include "../kernel/arch/x86_64/lapic.cpp"
#include "../kernel/arch/x86_64/smp.cpp"
#include "../kernel/util/logger.cpp"
#include "../kernel/main.cpp"

static uint8_t gFrameBuffer[300 * 32 * 4];
static EFI_GRAPHICS_OUTPUT_MODE_INFORMATION gInfo{};
static EFI_GRAPHICS_OUTPUT_PROTOCOL_MODE gMode{};
static EFI_GRAPHICS_OUTPUT_PROTOCOL gGop{};

static EFI_STATUS EFIAPI FakeLocateProtocol(EFI_GUID*, void*, void** interface_out) {
    *interface_out = &gGop;
    return EFI_SUCCESS;
}

static uint8_t* pixel_at(uint32_t x, uint32_t y) {
    const uint32_t idx = (y * gInfo.PixelsPerScanLine + x) * 4;
    return &gFrameBuffer[idx];
}

static bool contains_foreground() {
    const uint8_t expected_fg[3] = {255, 200, 120};   // foreground (BGR)
    const uint8_t expected_accent[3] = {220, 140, 80}; // accent (BGR)
    for (uint32_t y = 0; y < gInfo.VerticalResolution; ++y) {
        for (uint32_t x = 0; x < gInfo.HorizontalResolution; ++x) {
            uint8_t* px = pixel_at(x, y);
            bool is_fg = px[0] == expected_fg[0] && px[1] == expected_fg[1] && px[2] == expected_fg[2];
            bool is_accent = px[0] == expected_accent[0] && px[1] == expected_accent[1] && px[2] == expected_accent[2];
            if (is_fg || is_accent) {
                return true;
            }
        }
    }
    return false;
}

int main() {
    gInfo.HorizontalResolution = 300;
    gInfo.VerticalResolution = 32;
    gInfo.PixelFormat = PixelBlueGreenRedReserved8BitPerColor;
    gInfo.PixelsPerScanLine = 300;

    gMode.Info = &gInfo;
    gMode.FrameBufferBase = reinterpret_cast<EFI_PHYSICAL_ADDRESS>(gFrameBuffer);
    gMode.FrameBufferSize = sizeof(gFrameBuffer);
    gGop.Mode = &gMode;

    renderer_init(&gGop);
    renderer_clear(Rgb{0, 0, 0});

    // Outline rectangle should paint the border and leave the interior untouched.
    Rgb outline{1, 2, 3};
    renderer_rect_outline(1, 1, 4, 3, outline);
    uint8_t* top_left_outline = pixel_at(1, 1);
    assert(top_left_outline[0] == outline.b);
    assert(top_left_outline[1] == outline.g);
    assert(top_left_outline[2] == outline.r);
    uint8_t* bottom_right_outline = pixel_at(4, 3);
    assert(bottom_right_outline[0] == outline.b);
    assert(bottom_right_outline[1] == outline.g);
    assert(bottom_right_outline[2] == outline.r);
    // Interior should remain the cleared color (all zeros).
    uint8_t* interior = pixel_at(2, 2);
    assert(interior[0] == 0 && interior[1] == 0 && interior[2] == 0);

    // Shell input pipeline: typing, backspace, enter, help, clear.
    ShellConfig shell_cfg{
        .win_x = 8,
        .win_y = 4,
        .win_w = 120,
        .win_h = 64,
        .background = Rgb{0, 0, 0},
        .window = Rgb{0, 0, 0},
        .title_bar = Rgb{0, 0, 0},
        .foreground = Rgb{3, 3, 3},
        .accent = Rgb{4, 4, 4},
    };
    shell_init(shell_cfg);
    assert(shell_debug_input_len() == 0);
    assert(shell_debug_history_size() == 0);

    shell_handle_key('t');
    shell_handle_key('e');
    shell_handle_key('s');
    shell_handle_key('t');
    assert(shell_debug_input_len() == 4);

    shell_handle_key('\b');
    assert(shell_debug_input_len() == 3);

    shell_handle_key('\n');
    assert(shell_debug_input_len() == 0);
    assert(shell_debug_history_size() == 1);
    assert(shell_debug_history_line(0)[0] == 't');
    assert(shell_debug_history_line(0)[1] == 'e');
    assert(shell_debug_history_line(0)[2] == 's');
    assert(shell_debug_history_line(0)[3] == 0);

    shell_handle_key('h');
    shell_handle_key('e');
    shell_handle_key('l');
    shell_handle_key('p');
    shell_handle_key('\n');
    bool found_help = false;
    for (size_t i = 0; i < shell_debug_history_size(); ++i) {
        const char* line = shell_debug_history_line(i);
        if (line[0] == 'C' && line[1] == 'o') {
            found_help = true;
            break;
        }
    }
    assert(found_help);

    shell_handle_key('c');
    shell_handle_key('l');
    shell_handle_key('e');
    shell_handle_key('a');
    shell_handle_key('r');
    shell_handle_key('\n');
    assert(shell_debug_history_size() == 0);

    usb::usb_init();
    shell_handle_key('u');
    shell_handle_key('s');
    shell_handle_key('b');
    shell_handle_key('\n');
    assert(shell_debug_history_size() == 1);
    const char* usb_line = shell_debug_history_line(0);
    assert(usb_line[0] == 'U' && usb_line[1] == 'S' && usb_line[2] == 'B');

    EFI_BOOT_SERVICES services{};
    services.LocateProtocol = FakeLocateProtocol;

    EFI_SYSTEM_TABLE system{};
    system.BootServices = &services;

    EFI_STATUS status = efi_main(nullptr, &system);
    assert(status == EFI_SUCCESS);

    // Top-left pixel should be background (BGR order).
    uint8_t* top_left = pixel_at(0, 0);
    assert(top_left[0] == 24);
    assert(top_left[1] == 18);
    assert(top_left[2] == 18);

    // Foreground color must appear after drawing the message.
    assert(contains_foreground());

    // USB stack controllers/devices.
    assert(usb::controller_count() == 3);
    assert(usb::device_count() == 1);
    const usb_controller_t* c0 = usb::controller_at(0);
    const usb_controller_t* c1 = usb::controller_at(1);
    const usb_controller_t* c2 = usb::controller_at(2);
    assert(c0 && c0->type == USB_CONTROLLER_XHCI);
    assert(c1 && c1->type == USB_CONTROLLER_EHCI);
    assert(c2 && c2->type == USB_CONTROLLER_OHCI);
    assert(usb::controller_at(3) == nullptr);
    usb::usb_poll();
    assert(usb::controller_count() == 3);

    // MSI/MSI-X allocators and stubs.
    msi_allocator_reset();
    int first_vec = msi_allocator_next_vector();
    int second_vec = msi_allocator_next_vector();
    assert(first_vec == 32);
    assert(second_vec == 33);

    msi_config_t msi_cfg{};
    msi_init_config(&msi_cfg);
    assert(msi_cfg.vector != 0);
    assert(msi_cfg.masked == false);
    assert(msi_enable(&msi_cfg));
    assert(msi_disable(&msi_cfg));

    msix_entry_t msix_ent{};
    msix_init_entry(&msix_ent);
    assert(msix_ent.vector != 0);
    assert(msix_ent.masked == false);
    assert(msix_enable(&msix_ent));
    assert(msix_disable(&msix_ent));

    // xHCI legacy handoff and alignment helpers (MMIO base mocked to 0 under host).
    assert(xhci_legacy_handoff(0));
    assert(xhci_reset_with_delay(0, 0));
    assert(xhci_require_alignment(0x1000, 64));
    msi_config_t xhci_msi{};
    assert(xhci_configure_msi(&xhci_msi));
    assert(xhci_check_lowmem(0x100000));
    assert(xhci_route_ports(0));
    xhci_controller_t ctrl{};
    assert(xhci_controller_init(&ctrl, 0));
    uint8_t slot_id = 0;
    assert(xhci_cmd_enable_slot(&ctrl, &slot_id));
    assert(slot_id == 1);
    assert(xhci_cmd_address_device(&ctrl, slot_id, 0x1234));
    assert(xhci_cmd_configure_endpoint(&ctrl, slot_id, 0x5678));
    assert(xhci_poll_events(&ctrl) > 0);
    xhci_trb_ring_t tr;
    assert(xhci_ring_init(&tr, 8));
    usb_setup_packet_t setup{0x80, 6, 0x0100, 0, 18};
    usb_transfer_t xfer{};
    xfer.length = 18;
    assert(xhci_build_control_transfer(&tr, &xfer, &setup, 0));

    // Enumerated device descriptor should be present.
    const usb_device_t* dev0 = usb::controller_count() > 0 ? usb_stack_device_at(0) : nullptr;
    assert(dev0 != nullptr);
    const usb_device_descriptor_t* ddesc = usb_device_get_descriptor(dev0);
    assert(ddesc != nullptr);
    assert(ddesc->idVendor == 0x1234);
    assert(ddesc->idProduct == 0x5678);
    // Shell poll uses usb_poll(), so run once.
    usb::usb_poll();

    // HID keyboard: press/release 'a'.
    gKeyEvents = 0;
    hid_keyboard_state_t hk{};
    hid_keyboard_reset(&hk);
    uint8_t press[8] = {0, 0, 0x04, 0, 0, 0, 0, 0};  // 'a'
    hid_keyboard_handle_report(&hk, press, sizeof(press));
    uint8_t release[8] = {0, 0, 0, 0, 0, 0, 0, 0};
    hid_keyboard_handle_report(&hk, release, sizeof(release));
    assert(gKeyEvents >= 2);
    assert(gKeys[0] == 'a' && gPressed[0] == true);
    assert(gKeys[1] == 'a' && gPressed[1] == false);

    // HID mouse parsing.
    hid_mouse_state_t hm{};
    hid_mouse_reset(&hm);
    uint8_t mrep[3] = {1, 5, 0xFB};
    hid_mouse_handle_report(&hm, mrep, sizeof(mrep));
    assert(hm.buttons == 1);
    assert(hm.delta_x == 5);
    assert(hm.delta_y == (int8_t)0xFB);

    // PS/2 fallback keyboard (host-injected scancodes).
    gKeyEvents = 0;
    ps2::init();
    ps2::debug_inject_scancode(0x1E);  // 'a' press
    ps2::debug_inject_scancode(0x9E);  // 'a' release
    ps2::poll();
    assert(gKeyEvents >= 2);
    assert(gKeys[0] == 'a');
    assert(gPressed[0] == true);
    assert(gKeys[1] == 'a');
    assert(gPressed[1] == false);

    return 0;
}
