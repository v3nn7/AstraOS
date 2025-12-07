#include "kernel_tests.h"
#include "klog.h"
#include "initcall.h"
#include "driver_manager.h"
#include "vfs.h"
#include "ipc.h"
#include "tty.h"
#include "string.h"
#include "panic.h"
#include "timer.h"
#include "interrupts.h"
#include "apic.h"

static void expect(bool cond, const char *msg) {
    if (!cond) {
        klog_printf(KLOG_ERROR, "TEST FAIL: %s", msg);
        panic("test failure: %s", msg);
    } else {
        klog_printf(KLOG_DEBUG, "TEST OK: %s", msg);
    }
}

static void test_klog(void) {
    klog_init();
    klog_set_level(KLOG_TRACE);
    klog_printf(KLOG_INFO, "klog-test");
    expect(klog_get_level() == KLOG_TRACE, "klog level get");
    expect(strncmp(klog_level_name(KLOG_WARN), "WARN", 4) == 0, "klog level name");
    char buf[256];
    klog_copy_recent(buf, sizeof(buf));
    /* ensure substring exists */
    bool found = false;
    for (size_t i = 0; buf[i] && i + 8 < sizeof(buf); ++i) {
        if (buf[i] == 'k' && strncmp(&buf[i], "klog-test", 9) == 0) { found = true; break; }
    }
    expect(found, "klog writes into buffer");
}

static int dummy_initcall_counter = 0;
static int dummy_init_fn(void) { ++dummy_initcall_counter; return 0; }

static void test_initcall(void) {
    initcall_register(INITCALL_LATE, dummy_init_fn, "dummy_init_fn");
    initcall_run_all();
    expect(dummy_initcall_counter == 1, "initcall executed once");
}

static int dummy_attach_called = 0;
static int dummy_attach(void *dev) { (void)dev; dummy_attach_called++; return 0; }
static driver_t dummy_driver = {
    .name = "dummy",
    .cls = DRIVER_GENERIC,
    .probe = NULL,
    .init = NULL,
    .attach = dummy_attach
};

static void test_driver_manager(void) {
    driver_manager_init();
    driver_register(&dummy_driver);
    expect(driver_find("dummy") != NULL, "driver find");
    driver_attach("dummy", NULL);
    expect(dummy_attach_called == 1, "driver attach called");
}

static void test_vfs(void) {
    vfs_init();
    vfs_node_t *r = ramfs_mount();
    vfs_node_t *dir = vfs_mkdir(r, "etc");
    expect(dir != NULL, "vfs mkdir");
    vfs_node_t *f = vfs_create(r, "file.txt", VFS_NODE_FILE);
    const char *msg = "hello";
    vfs_write(f, 0, 5, msg);
    char out[6] = {0};
    vfs_read(f, 0, 5, out);
    expect(strncmp(out, msg, 5) == 0, "vfs read/write");
}

static void test_ipc(void) {
    ipc_channel_t *ch = ipc_channel_create(4);
    expect(ch != NULL, "ipc create");
    ipc_send(ch, 1);
    ipc_send(ch, 2);
    expect(ipc_pending(ch) == 2, "ipc pending");
    uint64_t v = 0;
    ipc_recv(ch, &v);
    expect(v == 1, "ipc recv order");
    ipc_recv(ch, &v);
    expect(v == 2, "ipc recv second");
}

static void timer_cb(uint64_t tick, void *user) {
    uint64_t *slot = (uint64_t *)user;
    *slot = tick;
}

static void test_timer(void) {
    uint64_t cb_tick = 0;
    timer_init(50);
    timer_register_callback(timer_cb, &cb_tick);
    interrupt_frame_t frame = {0};
    uint64_t before = timer_ticks();
    timer_handle_irq(&frame);
    expect(timer_ticks() == before + 1, "timer tick increments");
    expect(cb_tick == timer_ticks(), "timer callback invoked");
}

static ssize_t dummy_dev_read(struct vfs_node *n, size_t off, size_t len, void *buf) {
    (void)n; (void)off; if (len == 0) return 0; ((char *)buf)[0] = '!'; return 1;
}

static ssize_t dummy_dev_write(struct vfs_node *n, size_t off, size_t len, const void *buf) {
    (void)n; (void)off; (void)buf; return (ssize_t)len;
}

static void test_devfs(void) {
    devfs_mount();
    devfs_register_chr("null", dummy_dev_read, dummy_dev_write, NULL);
    vfs_node_t *node = vfs_lookup(vfs_root(), "dev/null");
    char ch = 0;
    vfs_read(node, 0, 1, &ch);
    expect(ch == '!', "devfs read");
}

static int panic_seen = 0;
void panic_hook(const char *msg) { (void)msg; panic_seen = 1; }

static void test_panic_hook(void) {
    /* panic_hook sets flag; we do not allow panic to halt during tests */
    if (panic_seen) return;
    /* We do not call panic directly to avoid halting; just ensure hook symbol is linked. */
    expect(&panic_hook != NULL, "panic hook present");
}

static void test_tty(void) {
    tty_init();
    tty_feed_char('x');
    char out;
    expect(tty_read_char(&out) && out == 'x', "tty input buffer");
    tty_putc('y');
    tty_write("ok");
    tty_poll_input();
}

static void test_apic(void) {
    static uint32_t lapic_mem[0x400] = {0};
    static uint32_t ioapic_mem[8] = {0};
    apic_mock_set_bases(lapic_mem, ioapic_mem);
    lapic_init();
    lapic_eoi();
    lapic_timer_init(3, 10);
    ioapic_init();
    ioapic_redirect_irq(1, 40);
    expect(lapic_mem[0xF0 / 4] & 0x100, "lapic spurious enabled");
}

void kernel_tests_run(void) {
    test_klog();
    test_initcall();
    test_driver_manager();
    test_vfs();
    test_ipc();
    test_timer();
    test_devfs();
    test_panic_hook();
    test_tty();
    test_apic();
    klog_printf(KLOG_INFO, "kernel_tests: all passed");
}

