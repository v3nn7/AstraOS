/*
 * PS/2 Keyboard Driver – Scan Code Set 1
 * AstraOS Implementation
 * 
 * Scan Code Set 1 is the most compatible set.
 * Most PS/2 controllers translate Set 2 to Set 1 automatically.
 * Break codes: make code with bit 7 set (0x80 | make_code).
 * Extended keys use 0xE0 prefix.
 */

#include "types.h"
#include "interrupts.h"
#include "drivers.h"
#include "kernel.h"

#define PS2_DATA    0x60
#define PS2_STATUS  0x64
#define PS2_CMD     0x64

#define PS2_STATUS_OUTPUT_FULL  0x01
#define PS2_STATUS_INPUT_FULL   0x02
#define PS2_STATUS_SYSTEM       0x04
#define PS2_STATUS_CMD_DATA     0x08
#define PS2_STATUS_AUX_OUTPUT   0x20
#define PS2_STATUS_TIMEOUT      0x40
#define PS2_STATUS_PARITY       0x80

#define BUF_SIZE 256
static char kbuf[BUF_SIZE];
static volatile uint8_t khead = 0, ktail = 0;

/* State flags */
static bool shift_left = false;
static bool shift_right = false;
static bool ctrl_left = false;
static bool ctrl_right = false;
static bool alt_left = false;
static bool alt_right = false;
static bool caps_lock = false;
static bool num_lock = false;
static bool scroll_lock = false;
static bool extended = false;
static bool break_code = false;

/* Helper macros */
#define is_shift() (shift_left || shift_right)
#define is_ctrl() (ctrl_left || ctrl_right)
#define is_alt() (alt_left || alt_right)

/* ----------------------------------------------- */
/* RING BUFFER                                      */
/* ----------------------------------------------- */
static inline void kbd_buf_push(char c) {
    uint8_t next = (khead + 1) % BUF_SIZE;
    if (next != ktail) {
        kbuf[khead] = c;
        khead = next;
    }
}

static inline bool kbd_buf_pop(char *out) {
    if (khead == ktail) return false;
    *out = kbuf[ktail];
    ktail = (ktail + 1) % BUF_SIZE;
    return true;
}

/* ----------------------------------------------- */
/* SCAN CODE SET 1 MAPS (Correct mapping)          */
/* ----------------------------------------------- */

/* Normal (non-shifted) keymap - Set 1 make codes */
static const char set1_normal[256] = {
    /* Letters - Set 1 scancodes */
    [0x1E] = 'a', [0x30] = 'b', [0x2E] = 'c', [0x20] = 'd', [0x12] = 'e',
    [0x21] = 'f', [0x22] = 'g', [0x23] = 'h', [0x17] = 'i', [0x24] = 'j',
    [0x25] = 'k', [0x26] = 'l', [0x32] = 'm', [0x31] = 'n', [0x18] = 'o',
    [0x19] = 'p', [0x10] = 'q', [0x13] = 'r', [0x1F] = 's', [0x14] = 't',
    [0x16] = 'u', [0x2F] = 'v', [0x11] = 'w', [0x2D] = 'x', [0x15] = 'y',
    [0x2C] = 'z',
    
    /* Numbers row - Set 1 scancodes */
    [0x02] = '1', [0x03] = '2', [0x04] = '3', [0x05] = '4', [0x06] = '5',
    [0x07] = '6', [0x08] = '7', [0x09] = '8', [0x0A] = '9', [0x0B] = '0',
    
    /* Symbols - Set 1 scancodes */
    [0x0C] = '-', [0x0D] = '=', [0x1A] = '[', [0x1B] = ']',
    [0x2B] = '\\', [0x27] = ';', [0x28] = '\'', [0x33] = ',',
    [0x34] = '.', [0x35] = '/', [0x29] = '`',
    
    /* Whitespace and control */
    [0x39] = ' ',  /* Space */
    [0x0F] = '\t', /* Tab */
    [0x1C] = '\n', /* Enter */
    [0x0E] = '\b', /* Backspace */
    [0x01] = 0x1B, /* Escape */
};

/* Shifted keymap */
static const char set1_shift[256] = {
    /* Letters (uppercase) */
    [0x1E] = 'A', [0x30] = 'B', [0x2E] = 'C', [0x20] = 'D', [0x12] = 'E',
    [0x21] = 'F', [0x22] = 'G', [0x23] = 'H', [0x17] = 'I', [0x24] = 'J',
    [0x25] = 'K', [0x26] = 'L', [0x32] = 'M', [0x31] = 'N', [0x18] = 'O',
    [0x19] = 'P', [0x10] = 'Q', [0x13] = 'R', [0x1F] = 'S', [0x14] = 'T',
    [0x16] = 'U', [0x2F] = 'V', [0x11] = 'W', [0x2D] = 'X', [0x15] = 'Y',
    [0x2C] = 'Z',
    
    /* Numbers row (shifted symbols) */
    [0x02] = '!', [0x03] = '@', [0x04] = '#', [0x05] = '$', [0x06] = '%',
    [0x07] = '^', [0x08] = '&', [0x09] = '*', [0x0A] = '(', [0x0B] = ')',
    
    /* Symbols (shifted) */
    [0x0C] = '_', [0x0D] = '+', [0x1A] = '{', [0x1B] = '}',
    [0x2B] = '|', [0x27] = ':', [0x28] = '"', [0x33] = '<',
    [0x34] = '>', [0x35] = '?', [0x29] = '~',
    
    /* Whitespace (same) */
    [0x39] = ' ', [0x0F] = '\t', [0x1C] = '\n', [0x0E] = '\b',
};

/* Extended keys (0xE0 prefix) - common ones */
static const char set1_extended[256] = {
    [0x1C] = '\n', /* Keypad Enter */
    [0x35] = '/',  /* Keypad / */
    [0x4A] = '\n', /* Keypad Enter (alternate) */
    [0x5A] = '\n', /* Keypad Enter (alternate) */
    [0x69] = '\b', /* End */
    [0x6B] = '\b', /* Left Arrow */
    [0x72] = '\b', /* Down Arrow */
    [0x74] = '\b', /* Right Arrow */
    [0x75] = '\b', /* Up Arrow */
    [0x7A] = '\b', /* Page Down */
    [0x7D] = '\b', /* Page Up */
    [0x70] = '\b', /* Insert */
    [0x71] = '\b', /* Delete */
    [0x6C] = '\b', /* Home */
    [0x79] = '\b', /* End */
};

/* ----------------------------------------------- */
/* TRANSLATION                                     */
/* ----------------------------------------------- */

static char translate_scancode_set1(uint8_t sc) {
    /* Handle extended keys */
    if (extended) {
        extended = false;
        char c = set1_extended[sc];
        if (c != 0) return c;
        /* Extended key not in map - ignore */
        return 0;
    }
    
    /* Get base character */
    char c = set1_normal[sc];
    if (c == 0) return 0;
    
    /* Apply shift/caps lock */
    bool use_shift = is_shift() ^ (caps_lock && (c >= 'a' && c <= 'z'));
    
    if (use_shift) {
        char shifted = set1_shift[sc];
        if (shifted != 0) return shifted;
    }
    
    return c;
}

/* ----------------------------------------------- */
/* PROCESS SCANCODE                                */
/* ----------------------------------------------- */

static void keyboard_process_scancode(uint8_t sc) {
    /* Debug: uncomment to see scancodes */
    // printf("keyboard: scancode 0x%02x (extended=%d, break=%d)\n", sc, extended, break_code);
    
    /* 0xE0 prefix for extended keys */
    if (sc == 0xE0) {
        extended = true;
        return;
    }
    
    /* Set 1: Break codes have bit 7 set (0x80 | make_code) */
    if (sc & 0x80) {
        uint8_t make_code = sc & 0x7F;
        
        /* Update modifier states */
        switch (make_code) {
            case 0x1D: ctrl_left = false; return;
            case 0x38: alt_left = false; return;
            case 0x2A: shift_left = false; return;
            case 0x36: shift_right = false; return;
            case 0x3A: /* Caps Lock release - already toggled on press */
            case 0x45: /* Num Lock release */
            case 0x46: /* Scroll Lock release */
                return;
        }
        
        /* Extended break codes */
        if (extended) {
            extended = false;
            switch (make_code) {
                case 0x1D: ctrl_right = false; return;
                case 0x38: alt_right = false; return;
            }
        }
        
        /* Ignore break codes for regular keys */
        return;
    }
    
    /* Handle make codes (key press) */
    
    /* Modifier keys - Set 1 scancodes */
    switch (sc) {
        case 0x1D: ctrl_left = true; return;
        case 0x38: alt_left = true; return;
        case 0x2A: shift_left = true; return;
        case 0x36: shift_right = true; return;
        case 0x3A: caps_lock = !caps_lock; return;
        case 0x45: num_lock = !num_lock; return;
        case 0x46: scroll_lock = !scroll_lock; return;
    }
    
    /* Extended modifier keys */
    if (extended) {
        extended = false;
        switch (sc) {
            case 0x1D: ctrl_right = true; return;
            case 0x38: alt_right = true; return;
            default:
                /* Other extended keys - translate */
                break;
        }
    }
    
    /* Translate scancode to character */
    char c = translate_scancode_set1(sc);
    if (c != 0) {
        // printf("keyboard: scancode 0x%02x -> '%c' (shift=%d, caps=%d)\n", sc, c, is_shift(), caps_lock);
        kbd_buf_push(c);
    }
}

/* ----------------------------------------------- */
/* IRQ HANDLER                                     */
/* ----------------------------------------------- */

static void keyboard_irq(interrupt_frame_t *f) {
    (void)f;
    
    uint8_t status = inb(PS2_STATUS);
    
    /* Check for errors */
    if (status & PS2_STATUS_TIMEOUT) {
        (void)inb(PS2_DATA);
        return;
    }
    
    /* Check if data is available */
    if (!(status & PS2_STATUS_OUTPUT_FULL)) {
        return;
    }
    
    /* Check if data is from mouse (aux device) */
    if (status & PS2_STATUS_AUX_OUTPUT) {
        (void)inb(PS2_DATA);
        return;
    }
    
    /* Read scancode */
    uint8_t scancode = inb(PS2_DATA);
    
    /* Process scancode */
    keyboard_process_scancode(scancode);
}

/* ----------------------------------------------- */
/* INITIALIZATION                                   */
/* ----------------------------------------------- */

static inline bool wait_input_clear(void) {
    for (int i = 0; i < 1000000; i++) {
        if (!(inb(PS2_STATUS) & PS2_STATUS_INPUT_FULL)) {
            return true;
        }
        for (volatile int j = 0; j < 100; j++);
    }
    return false;
}

static inline bool wait_output_ready(void) {
    for (int i = 0; i < 1000000; i++) {
        if (inb(PS2_STATUS) & PS2_STATUS_OUTPUT_FULL) {
            return true;
        }
        for (volatile int j = 0; j < 100; j++);
    }
    return false;
}

void keyboard_init(void) {
    printf("keyboard: initializing PS/2 keyboard (Scan Code Set 1)\n");
    
    /* Initialize state */
    khead = ktail = 0;
    shift_left = shift_right = false;
    ctrl_left = ctrl_right = false;
    alt_left = alt_right = false;
    caps_lock = num_lock = scroll_lock = false;
    extended = false;
    break_code = false;
    
    /* Flush output buffer */
    int flushed = 0;
    while ((inb(PS2_STATUS) & PS2_STATUS_OUTPUT_FULL) && flushed < 100) {
        (void)inb(PS2_DATA);
        flushed++;
    }
    if (flushed > 0) {
        printf("keyboard: flushed %d bytes\n", flushed);
    }
    
    /* Most PS/2 controllers translate Set 2 to Set 1 automatically */
    /* We'll use Set 1 which is what the controller outputs */
    printf("keyboard: using Scan Code Set 1 (controller translation)\n");
    
    /* Register IRQ handler */
    irq_register_handler(1, keyboard_irq);
    printf("keyboard: IRQ handler registered (IRQ1 -> vector 33)\n");
    
    printf("keyboard: initialization complete\n");
    printf("keyboard: DEBUG MODE ENABLED - scancodes will be printed\n");
}

/* ----------------------------------------------- */
/* PUBLIC API                                       */
/* ----------------------------------------------- */

bool keyboard_available(void) {
    return khead != ktail;
}

bool keyboard_read_char(char *out) {
    return kbd_buf_pop(out);
}

bool keyboard_poll_char(char *out) {
    /* Polling should not be used when IRQ is working */
    /* This function is kept for compatibility but should not be called */
    return keyboard_read_char(out);
}
