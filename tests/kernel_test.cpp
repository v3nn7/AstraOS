#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

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
extern "C" uint64_t rdmsr(uint32_t) { return 0; }
extern "C" void wrmsr(uint32_t, uint64_t) {}

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
#include "../kernel/drivers/usb/usb_core.cpp"
#include "../kernel/drivers/usb/xhci.cpp"
#include "../kernel/drivers/usb/usb_hid.cpp"
#include "../kernel/drivers/usb/usb_keyboard.cpp"
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

    // USB keyboard mapping and report handling.
    gKeyEvents = 0;
    usb::UsbDevice dev;
    usb::UsbConfiguration* usb_cfg = dev.configuration();
    usb_cfg->interface_count = 1;
    usb_cfg->interfaces[0].endpoint_count = 1;
    usb_cfg->interfaces[0].endpoints[0].type = usb::UsbTransferType::Interrupt;
    usb_cfg->interfaces[0].endpoints[0].address = 0x81;  // IN endpoint 1
    usb_cfg->interfaces[0].endpoints[0].attributes = 0x03;
    usb_cfg->interfaces[0].endpoints[0].max_packet_size = 8;
    usb_cfg->interfaces[0].endpoints[0].interval = 1;
    dev.set_slot_id(1);
    dev.set_speed(usb::UsbSpeed::Full);
    dev.set_configuration(1);

    usb::XhciController ctrl;
    assert(ctrl.init());

    usb::UsbKeyboardDriver driver;
    driver.bind(&ctrl, &dev);
    assert(driver.usage_to_key(0x1E) == '1');

    uint8_t press[8] = {0, 0, 0x04, 0, 0, 0, 0, 0};  // 'a'
    driver.handle_report(press);
    assert(gKeyEvents >= 1);
    assert(gKeys[0] == 'a');
    assert(gPressed[0] == true);

    uint8_t release[8] = {0, 0, 0, 0, 0, 0, 0, 0};
    driver.handle_report(release);
    assert(gKeyEvents >= 2);
    assert(gKeys[1] == 'a');
    assert(gPressed[1] == false);

    return 0;
}
