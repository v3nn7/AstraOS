#include "stddef.h"
#include "types.h"
#include "drivers.h"
#include "interrupts.h"
#include "event.h"
#include "kernel.h"

#define PS2_DATA   0x60
#define PS2_STATUS 0x64
#define PS2_CMD    0x64

#define BUF_SIZE 128

static uint8_t sc_buf[BUF_SIZE];
static volatile uint8_t head = 0, tail = 0;
static bool shift = false;
static bool extended = false;

/* Classic scancode set 1 (which your controller translates to by default) */
static const char keymap[128] = {
    0,27,'1','2','3','4','5','6','7','8','9','0','-','=', '\b',
    '\t','q','w','e','r','t','y','u','i','o','p','[',']','\n',0,
    'a','s','d','f','g','h','j','k','l',';','\'','`',0,'\\',
    'z','x','c','v','b','n','m',',','.','/',0,'*',0,' ',
};

static const char keymap_shift_state[128] = {
    0,27,'!','@','#','$','%','^','&','*','(',')','_','+', '\b',
    '\t','Q','W','E','R','T','Y','U','I','O','P','{','}','\n',0,
    'A','S','D','F','G','H','J','K','L',':','"','~',0,'|',
    'Z','X','C','V','B','N','M','<','>','?',0,'*',0,' ',
};

/* --------------------------------------------------------------------- */
/* BUFFER HANDLING */
/* --------------------------------------------------------------------- */

static inline void push_sc(uint8_t sc) {
    uint8_t next = (head + 1) % BUF_SIZE;
    if (next != tail) {
        sc_buf[head] = sc;
        head = next;
    }
}

static inline bool pop_sc(uint8_t *out) {
    if (head == tail) return false;
    *out = sc_buf[tail];
    tail = (tail + 1) % BUF_SIZE;
    return true;
}

/* --------------------------------------------------------------------- */
/* SCANCODE PROCESSING */
/* --------------------------------------------------------------------- */

static inline char translate_sc(uint8_t sc) {
    if (sc >= 128) return 0;
    char c = shift ? keymap_shift_state[sc] : keymap[sc];
    return c;
}

static void handle_scancode(uint8_t sc) {
    /* ACK / resend */
    if (sc == 0xFA || sc == 0xFE) return;

    /* Extended prefix */
    if (sc == 0xE0) { extended = true; return; }

    /* Break code (key release) */
    if (sc & 0x80) {
        uint8_t key = sc & 0x7F;

        /* Shift released */
        if (key == 0x2A || key == 0x36)
            shift = false;
        extended = false;
        return;
    }

    /* If extended, ignore for now (arrows/F-keys not mapped) */
    if (extended) { extended = false; return; }

    /* Shift pressed */
    if (sc == 0x2A || sc == 0x36) {
        shift = true;
        return;
    }

    char c = translate_sc(sc);
    if (c) {
        push_sc(sc);
        gui_event_push_keychar(c);
    }
}

/* --------------------------------------------------------------------- */
/* IRQ HANDLER */
/* --------------------------------------------------------------------- */

static void keyboard_irq(interrupt_frame_t *f) {
    (void)f;

    uint8_t st = inb(PS2_STATUS);

    /* Mouse data? ignore. */
    if (st & 0x20) { (void)inb(PS2_DATA); return; }

    if (!(st & 0x01)) return;

    uint8_t sc = inb(PS2_DATA);
    handle_scancode(sc);
}

/* --------------------------------------------------------------------- */
/* INIT */
/* --------------------------------------------------------------------- */

static inline bool wait_input_clear(void) {
    for (int i = 0; i < 100000; i++) {
        if (!(inb(PS2_STATUS) & 0x02)) return true;
    }
    return false; /* timeout */
}

static inline bool wait_output_ready(void) {
    for (int i = 0; i < 100000; i++) {
        if (inb(PS2_STATUS) & 0x01) return true;
    }
    return false; /* timeout */
}

static bool send_cmd(uint8_t cmd, uint8_t *resp) {
    if (!wait_input_clear()) return false;
    outb(PS2_DATA, cmd);
    if (!wait_output_ready()) return false;
    uint8_t r = inb(PS2_DATA);
    if (resp) *resp = r;
    return (r == 0xFA);
}

void keyboard_init(void) {
    printf("keyboard: initializing\n");
    head = tail = 0;
    shift = false;
    extended = false;

    /* Flush output buffer */
    int flush_count = 0;
    while ((inb(PS2_STATUS) & 0x01) && flush_count < 100) {
        (void)inb(PS2_DATA);
        flush_count++;
    }
    printf("keyboard: flushed %d bytes\n", flush_count);

    /* Disable then re-enable port 1 (keyboard) */
    if (!wait_input_clear()) { printf("keyboard: timeout waiting for input clear (disable)\n"); return; }
    outb(PS2_CMD, 0xAD);
    if (!wait_input_clear()) { printf("keyboard: timeout waiting for input clear (enable)\n"); return; }
    outb(PS2_CMD, 0xAE);
    printf("keyboard: port enabled\n");

    /* Enable IRQ1 in controller config */
    if (!wait_input_clear()) { printf("keyboard: timeout reading config\n"); return; }
    outb(PS2_CMD, 0x20);
    if (!wait_output_ready()) { printf("keyboard: timeout waiting for config response\n"); return; }
    uint8_t cfg = inb(PS2_DATA);
    cfg |= 1;           /* enable keyboard IRQ */
    cfg &= ~(1 << 6);   /* leave translation on (set1) */
    if (!wait_input_clear()) { printf("keyboard: timeout writing config\n"); return; }
    outb(PS2_CMD, 0x60);
    if (!wait_input_clear()) { printf("keyboard: timeout writing config data\n"); return; }
    outb(PS2_DATA, cfg);
    printf("keyboard: config updated (cfg=0x%02x)\n", cfg);

    /* Reset keyboard - skip if it fails */
    uint8_t resp = 0;
    if (!send_cmd(0xFF, &resp)) {
        printf("keyboard: reset no ACK (resp=0x%02x), continuing anyway\n", resp);
    } else {
        if (wait_output_ready()) {
            uint8_t bat = inb(PS2_DATA);
            if (bat != 0xAA) printf("keyboard: BAT fail (0x%02x), continuing anyway\n", bat);
            else printf("keyboard: BAT OK\n");
        }
    }

    /* Set scancode set 1 (controller translation expects this) - skip if fails */
    if (send_cmd(0xF0, &resp) && resp == 0xFA) {
        send_cmd(0x01, &resp);
        printf("keyboard: scancode set 1 configured\n");
    } else {
        printf("keyboard: scancode set config skipped\n");
    }

    /* Enable scanning - skip if fails */
    if (!send_cmd(0xF4, NULL)) {
        printf("keyboard: enable scanning failed, continuing anyway\n");
    } else {
        printf("keyboard: scanning enabled\n");
    }

    /* Unmask IRQ1 on PIC - but we're using APIC, so this may not be needed */
    /* Skip PIC unmask in APIC mode - IOAPIC handles IRQ routing */
    printf("keyboard: skipping PIC unmask (using APIC/IOAPIC)\n");

    printf("keyboard: registering IRQ handler\n");
    irq_register_handler(1, keyboard_irq);
    printf("keyboard: IRQ handler registered\n");

    printf("PS2: keyboard ready\n");
}

/* --------------------------------------------------------------------- */
/* API */
/* --------------------------------------------------------------------- */

bool keyboard_pop_char(char *out) {
    uint8_t sc;
    if (!pop_sc(&sc)) return false;
    char c = translate_sc(sc);
    if (c) {
        *out = c;
        return true;
    }
    /* If translation failed, try next scancode */
    return keyboard_pop_char(out);
}

bool keyboard_has_data(void) {
    return head != tail;
}

bool keyboard_pop_scancode(uint8_t *code_out) {
    return pop_sc(code_out);
}

bool keyboard_poll_char(char *out) {
    uint8_t st = inb(PS2_STATUS);
    if (!(st & 0x01) || (st & 0x20)) return false; /* no data or mouse data */
    uint8_t sc = inb(PS2_DATA);
    handle_scancode(sc);
    return keyboard_pop_char(out);
}
