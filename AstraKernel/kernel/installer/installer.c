#include "types.h"
#include "drivers.h"
#include "event.h"
#include "ui.h"
#include "installer.h"
#include "string.h"

/* Simple workflow states */
typedef enum {
    INS_WELCOME = 0,
    INS_DISK,
    INS_CONFIRM,
    INS_PROGRESS,
    INS_DONE
} ins_state_t;

static const char *disks[] = { "/dev/sda (default)" };
static int selected_disk = 0;

static int progress = 0;
static const char *progress_msg = "Preparing...";

static void draw_header(void) {
    ui_rect(0, 0, fb_width(), 28, 0xFF1E3A5F);
    ui_text(8, 8, "AstraInstaller", 0xFFFFFFFF, 0xFF1E3A5F);
}

static void draw_welcome(void) {
    ui_clear(0xFF0F1115);
    draw_header();
    ui_text(40, 60, "Welcome to AstraInstaller", 0xFFE0E0E0, 0xFF0F1115);
    ui_text(40, 80, "This will install AstraOS onto the selected disk.", 0xFFA0A0A0, 0xFF0F1115);
}

static bool screen_welcome(gui_event_t *ev) {
    ui_button_t next = { .x = (int)fb_width() - 140, .y = (int)fb_height() - 60, .w = 120, .h = 32, .text = "Next" };
    draw_welcome();
    return ui_button(&next, ev, 0xFF2E7D32, 0xFFFFFFFF);
}

static bool screen_disk(gui_event_t *ev) {
    ui_clear(0xFF0F1115);
    draw_header();
    ui_text(40, 60, "Select target disk", 0xFFE0E0E0, 0xFF0F1115);
    ui_listview_t lv = { .items = disks, .count = 1, .selected = selected_disk, .scroll = 0, .visible = 5 };
    bool changed = ui_listview(&lv, ev, 40, 90, (int)fb_width() - 80, 120, 0xFF141820, 0xFF1E3A5F, 0xFFE0E0E0);
    if (changed) selected_disk = lv.selected;
    ui_button_t back = { 40, (int)fb_height() - 60, 100, 32, "Back" };
    ui_button_t next = { (int)fb_width() - 140, (int)fb_height() - 60, 120, 32, "Next" };
    bool b = ui_button(&back, ev, 0xFF444444, 0xFFFFFFFF);
    bool n = ui_button(&next, ev, 0xFF2E7D32, 0xFFFFFFFF);
    if (b) return false; /* signal go back */
    if (n) return true;  /* signal next */
    return -1; /* stay */
}

static bool screen_confirm(gui_event_t *ev) {
    ui_clear(0xFF0F1115);
    draw_header();
    ui_text(40, 60, "Confirm installation", 0xFFE0E0E0, 0xFF0F1115);
    ui_text(40, 90, "Disk: /dev/sda", 0xFFA0A0A0, 0xFF0F1115);
    ui_text(40, 110, "Actions:", 0xFFA0A0A0, 0xFF0F1115);
    ui_text(60, 130, "- Create filesystem (ext2)", 0xFFA0A0A0, 0xFF0F1115);
    ui_text(60, 145, "- Copy system image", 0xFFA0A0A0, 0xFF0F1115);
    ui_text(60, 160, "- Install Limine (UEFI+BIOS)", 0xFFA0A0A0, 0xFF0F1115);

    ui_button_t back = { 40, (int)fb_height() - 60, 100, 32, "Back" };
    ui_button_t install = { (int)fb_width() - 180, (int)fb_height() - 60, 160, 32, "Install" };
    bool b = ui_button(&back, ev, 0xFF444444, 0xFFFFFFFF);
    bool i = ui_button(&install, ev, 0xFF2E7D32, 0xFFFFFFFF);
    if (b) return false; /* back */
    if (i) return true;  /* go install */
    return -1;
}

static void simulate_step(const char *msg, int amount) {
    progress_msg = msg;
    for (int i = 0; i < amount; ++i) {
        if (progress < 100) progress++;
        ui_clear(0xFF0F1115);
        draw_header();
        ui_text(40, 60, progress_msg, 0xFFE0E0E0, 0xFF0F1115);
        ui_progress(40, 100, (int)fb_width() - 80, 20, progress, 0xFF2E7D32, 0xFF141820, 0xFF444444);
        // crude delay
        for (volatile int d = 0; d < 500000; ++d) { __asm__ volatile("nop"); }
    }
}

static void screen_progress(void) {
    progress = 0;
    simulate_step("Partitioning /dev/sda ...", 20);
    simulate_step("Creating EXT2 filesystem ...", 20);
    simulate_step("Copying system image ...", 40);
    simulate_step("Installing Limine bootloader ...", 20);
    progress = 100;
    progress_msg = "Done.";
    ui_clear(0xFF0F1115);
    draw_header();
    ui_text(40, 60, "Installation complete.", 0xFFE0E0E0, 0xFF0F1115);
    ui_progress(40, 100, (int)fb_width() - 80, 20, progress, 0xFF2E7D32, 0xFF141820, 0xFF444444);
}

static bool screen_done(gui_event_t *ev) {
    ui_text(40, 140, "You can reboot now.", 0xFFA0A0A0, 0xFF0F1115);
    ui_button_t reboot = { (int)fb_width() - 160, (int)fb_height() - 60, 140, 32, "Reboot" };
    return ui_button(&reboot, ev, 0xFF2E7D32, 0xFFFFFFFF);
}

void installer_run(void) {
    ins_state_t st = INS_WELCOME;
    gui_event_t ev;

    for (;;) {
        while (!gui_event_poll(&ev)) {
            // idle
        }

        if (st == INS_WELCOME) {
            if (screen_welcome(&ev)) st = INS_DISK;
        }
        else if (st == INS_DISK) {
            bool res = screen_disk(&ev);
            if (res == false) st = INS_WELCOME;
            else if (res == true) st = INS_CONFIRM;
        }
        else if (st == INS_CONFIRM) {
            bool res = screen_confirm(&ev);
            if (res == false) st = INS_DISK;
            else if (res == true) {
                st = INS_PROGRESS;
                screen_progress();
                st = INS_DONE;
            }
        }
        else if (st == INS_DONE) {
            if (screen_done(&ev)) {
                // simple halt after "reboot"
                for(;;) __asm__ volatile("hlt");
            }
        }
    }
}

