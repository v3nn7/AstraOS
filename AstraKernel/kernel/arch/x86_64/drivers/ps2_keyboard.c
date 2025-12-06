#include "types.h"
#include "drivers.h"
#include "interrupts.h"

#define PS2_DATA   0x60
#define PS2_STATUS 0x64
#define PS2_CMD    0x64

#define BUF_SIZE 128

static uint8_t scancode_buf[BUF_SIZE];
static volatile uint8_t head, tail;
static bool shift;

static const char keymap[128] = {
    0,  27, '1','2','3','4','5','6','7','8','9','0','-','=', '\b',
    '\t','q','w','e','r','t','y','u','i','o','p','[',']','\n', 0,
    'a','s','d','f','g','h','j','k','l',';','\'','`', 0,'\\',
    'z','x','c','v','b','n','m',',','.','/', 0, '*', 0,' ',
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0
};

static const char keymap_shift[128] = {
    0,  27, '!','@','#','$','%','^','&','*','(',')','_','+', '\b',
    '\t','Q','W','E','R','T','Y','U','I','O','P','{','}','\n', 0,
    'A','S','D','F','G','H','J','K','L',':','\"','~', 0,'|',
    'Z','X','C','V','B','N','M','<','>','?', 0, '*', 0,' ',
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0
};

static void buf_push(uint8_t sc) {
    uint8_t next = (head + 1) % BUF_SIZE;
    if (next != tail) {
        scancode_buf[head] = sc;
        head = next;
    }
}

static bool buf_pop(uint8_t *out) {
    if (head == tail) return false;
    *out = scancode_buf[tail];
    tail = (tail + 1) % BUF_SIZE;
    return true;
}

static char translate(uint8_t sc) {
    if (sc >= 128) return 0;
    return shift ? keymap_shift[sc] : keymap[sc];
}

static void keyboard_irq(interrupt_frame_t *frame) {
    (void)frame;
    uint8_t sc = inb(PS2_DATA);
    if (sc == 0x2A || sc == 0x36) { shift = true; return; }
    if (sc == 0xAA || sc == 0xB6) { shift = false; return; }
    if (sc & 0x80) return;
    buf_push(sc);
}

static void wait_input(void) {
    while (inb(PS2_STATUS) & 0x02) { }
}

static void wait_output(void) {
    while (!(inb(PS2_STATUS) & 0x01)) { }
}

void keyboard_init(void) {
    head = tail = 0;
    shift = false;
    wait_input();
    outb(PS2_CMD, 0xAD); /* disable port 1 */
    wait_input();
    outb(PS2_CMD, 0xAE); /* enable port 1 */
    wait_input();
    outb(PS2_CMD, 0x20);
    wait_output();
    uint8_t config = inb(PS2_DATA);
    config |= 1; /* enable IRQ1 */
    wait_input();
    outb(PS2_CMD, 0x60);
    wait_input();
    outb(PS2_DATA, config);
    irq_register_handler(1, keyboard_irq);
}

bool keyboard_has_data(void) {
    return head != tail;
}

bool keyboard_pop_scancode(uint8_t *code_out) {
    return buf_pop(code_out);
}

bool keyboard_pop_char(char *ch_out) {
    uint8_t sc;
    while (buf_pop(&sc)) {
        char c = translate(sc);
        if (c) { *ch_out = c; return true; }
    }
    return false;
}
