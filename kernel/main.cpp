// Minimal UEFI framebuffer demo kernel for AstraOS 0.0.1.1exp
#include <stddef.h>
#include <stdint.h>

#include "arch/x86_64/gdt.hpp"
#include "arch/x86_64/idt.hpp"
#include "arch/x86_64/regs.hpp"
#include "arch/x86_64/smp.hpp"
#include "ui/renderer.hpp"
#include "ui/shell.hpp"
#include "util/logger.hpp"
#include "efi/gop.hpp"
#include <drivers/usb/usb_core.h>
#include <drivers/usb/usb_core.hpp>
#include <drivers/input/ps2/ps2.hpp>
#include <drivers/serial.hpp>

#define EFIAPI __attribute__((ms_abi))

extern "C" void serial_write(const char* s);
extern "C" void serial_init(void);

extern "C" {

using EFI_STATUS = uint64_t;
using EFI_HANDLE = void*;
using EFI_PHYSICAL_ADDRESS = uint64_t;

struct EFI_GUID {
    uint32_t Data1;
    uint16_t Data2;
    uint16_t Data3;
    uint8_t Data4[8];
};

struct EFI_TABLE_HEADER {
    uint64_t Signature;
    uint32_t Revision;
    uint32_t HeaderSize;
    uint32_t CRC32;
    uint32_t Reserved;
};

// Forward declarations for function pointer types we call.
using EFI_LOCATE_PROTOCOL = EFI_STATUS(EFIAPI*)(EFI_GUID* Protocol, void* Registration, void** Interface);
using EFI_EXIT_BOOT_SERVICES = EFI_STATUS(EFIAPI*)(EFI_HANDLE ImageHandle, uint64_t MapKey);
using EFI_EXIT = EFI_STATUS(EFIAPI*)(EFI_HANDLE ImageHandle, EFI_STATUS ExitStatus, size_t ExitDataSize, void* ExitData);

struct EFI_BOOT_SERVICES {
    EFI_TABLE_HEADER Hdr;
    void* RaiseTPL;
    void* RestoreTPL;
    void* AllocatePages;
    void* FreePages;
    void* GetMemoryMap;
    void* AllocatePool;
    void* FreePool;
    void* CreateEvent;
    void* SetTimer;
    void* WaitForEvent;
    void* SignalEvent;
    void* CloseEvent;
    void* CheckEvent;
    void* InstallProtocolInterface;
    void* ReinstallProtocolInterface;
    void* UninstallProtocolInterface;
    void* HandleProtocol;
    void* Reserved;
    void* RegisterProtocolNotify;
    void* LocateHandle;
    void* LocateDevicePath;
    void* InstallConfigurationTable;
    void* LoadImage;
    void* StartImage;
    EFI_EXIT Exit;
    void* UnloadImage;
    EFI_EXIT_BOOT_SERVICES ExitBootServices;
    void* GetNextMonotonicCount;
    void* Stall;
    void* SetWatchdogTimer;
    void* ConnectController;
    void* DisconnectController;
    void* OpenProtocol;
    void* CloseProtocol;
    void* OpenProtocolInformation;
    void* ProtocolsPerHandle;
    void* LocateHandleBuffer;
    EFI_LOCATE_PROTOCOL LocateProtocol;
    void* InstallMultipleProtocolInterfaces;
    void* UninstallMultipleProtocolInterfaces;
    void* CalculateCrc32;
    void* CopyMem;
    void* SetMem;
    void* CreateEventEx;
};

struct EFI_SYSTEM_TABLE {
    EFI_TABLE_HEADER Hdr;
    char16_t* FirmwareVendor;
    uint32_t FirmwareRevision;
    EFI_HANDLE ConsoleInHandle;
    void* ConIn;
    EFI_HANDLE ConsoleOutHandle;
    void* ConOut;
    EFI_HANDLE StandardErrorHandle;
    void* StdErr;
    void* RuntimeServices;
    EFI_BOOT_SERVICES* BootServices;
    size_t NumberOfTableEntries;
    void* ConfigurationTable;
};

}  // extern "C"

constexpr EFI_STATUS EFI_SUCCESS = 0;

// Standard GOP GUID.
static EFI_GUID kGraphicsOutputProtocolGuid{
    0x9042a9de, 0x23dc, 0x4a38, {0x96, 0xfb, 0x7a, 0xde, 0xd0, 0x80, 0x51, 0x6a}};

[[maybe_unused]] static void halt_forever() {
#ifdef HOST_TEST
    // In host tests we cannot execute HLT, so just return.
    return;
#else
    while (true) {
        __asm__ __volatile__("hlt");
    }
#endif
}

static void draw_splash() {
    Rgb bg{18, 18, 24};
    Rgb fg{120, 200, 255};
    renderer_clear(bg);
    const uint32_t screen_w = renderer_width();
    const uint32_t screen_h = renderer_height();
    const char* splash = "==[ ASTRA ]==";
    uint32_t x = (screen_w > 12 * 8) ? (screen_w - 12 * 8) / 2 : 0;
    uint32_t y = (screen_h > 16) ? (screen_h - 16) / 2 : 0;
    renderer_text(splash, x, y, fg, bg);
#ifndef HOST_TEST
    for (volatile int i = 0; i < 2000000; ++i) {
        __asm__ __volatile__("pause");
    }
#endif
}

static void render_shell() {
    Rgb background{18, 18, 24};
    Rgb window{28, 28, 36};
    Rgb title_bar{40, 40, 55};
    Rgb foreground{120, 200, 255};
    Rgb accent{80, 140, 220};

    const uint32_t screen_w = renderer_width();
    const uint32_t screen_h = renderer_height();

    const uint32_t win_w = (screen_w > 640) ? 640 : screen_w - 32;
    const uint32_t win_h = (screen_h > 240) ? 240 : screen_h - 32;
    const uint32_t win_x = (screen_w - win_w) / 2;
    const uint32_t win_y = (screen_h - win_h) / 2;

    ShellConfig cfg{
        .win_x = win_x,
        .win_y = win_y,
        .win_w = win_w,
        .win_h = win_h,
        .background = background,
        .window = window,
        .title_bar = title_bar,
        .foreground = foreground,
        .accent = accent,
    };

    shell_init(cfg);

    // Demonstrate a couple of prompt interactions to leave visible content.
    shell_handle_key('h');
    shell_handle_key('e');
    shell_handle_key('l');
    shell_handle_key('p');
    shell_handle_key('\n');
    shell_handle_key('b');
    shell_handle_key('o');
    shell_handle_key('o');
    shell_handle_key('t');
    shell_handle_key('e');
    shell_handle_key('d');
    shell_handle_key(' ');
    shell_handle_key('o');
    shell_handle_key('k');
    shell_handle_key('\n');

    shell_render();
}

// #region agent log
static bool dbg_serial_ready = false;
static inline void dbg_ensure_serial() {
    if (!dbg_serial_ready) {
        serial_init();
        dbg_serial_ready = true;
    }
}

static inline void dbg_hex16(char* out, uint64_t v) {
    out[0] = '0'; out[1] = 'x';
    for (int i = 0; i < 16; ++i) {
        uint8_t nib = (v >> (60 - 4 * i)) & 0xF;
        out[2 + i] = (nib < 10) ? ('0' + nib) : ('a' + nib - 10);
    }
    out[18] = '\0';
}

static inline void dbg_log_main(const char* location, const char* message, const char* hypo,
                                uint64_t key_val, const char* key_name, const char* runId) {
    dbg_ensure_serial();
    char hex[19]; dbg_hex16(hex, key_val);
    char buf[256];
    int p = 0;
    auto append = [&](const char* s) {
        while (*s && p < (int)(sizeof(buf) - 1)) buf[p++] = *s++;
    };
    append("{\"sessionId\":\"debug-session\",\"runId\":\"");
    append(runId);
    append("\",\"hypothesisId\":\"");
    append(hypo);
    append("\",\"location\":\"");
    append(location);
    append("\",\"message\":\"");
    append(message);
    append("\",\"data\":{\"");
    append(key_name);
    append("\":\"");
    append(hex);
    append("\"},\"timestamp\":0}");
    buf[p] = '\0';
    serial_write(buf);
    serial_write("\n");
}
// #endregion

extern "C" void kmain(EFI_GRAPHICS_OUTPUT_PROTOCOL* gop) {
#ifndef HOST_TEST
    __asm__ __volatile__("cli");
    init_gdt();
    init_idt();
    ps2::init();  // PS/2 fallback to release BIOS locks on some laptops
#endif
    dbg_log_main("main.cpp:kmain:start", "kmain_enter", "H1", (uint64_t)gop, "gop_ptr", "run-pre");
    renderer_init(gop);
    logger_init();
    draw_splash();
    render_shell();
    smp::init();
    dbg_log_main("main.cpp:kmain:usb", "before_usb_init", "H1", 0, "stage", "run-pre");
    usb::usb_init();
    dbg_log_main("main.cpp:kmain:usb", "after_usb_init", "H1", 0, "stage", "run-pre");
#ifndef HOST_TEST
    interrupts_enable();
    while (true) {
        ps2::poll();
        usb::usb_poll();
        shell_blink_tick();
        static uint32_t hb = 0;
        if ((hb++ % 5000u) == 0) {
            klog("main: heartbeat");
        }
        __asm__ __volatile__("hlt");
    }
#else
    ps2::poll();
    usb::usb_poll();
    shell_blink_tick();
#endif
}

extern "C" EFI_STATUS EFIAPI efi_main(EFI_HANDLE /*image_handle*/, EFI_SYSTEM_TABLE* system_table) {
    EFI_GRAPHICS_OUTPUT_PROTOCOL* gop = nullptr;
    EFI_STATUS status = system_table->BootServices->LocateProtocol(&kGraphicsOutputProtocolGuid, nullptr,
                                                                   reinterpret_cast<void**>(&gop));
    if (status != EFI_SUCCESS || gop == nullptr) {
        return status;
    }
    kmain(gop);
    return EFI_SUCCESS;
}
