#include <assert.h>
#include <stddef.h>
#include <stdint.h>

// Reuse the kernel implementation for host-side checks.
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
    return 0;
}
