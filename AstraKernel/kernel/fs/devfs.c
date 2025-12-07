#include "vfs.h"
#include "kmalloc.h"
#include "klog.h"

typedef struct {
    vfs_read_t rd;
    vfs_write_t wr;
    void *ctx;
} devfs_entry_t;

static vfs_node_t *dev_root = NULL;

static ssize_t dev_read(vfs_node_t *node, size_t off, size_t len, void *buf) {
    devfs_entry_t *e = (devfs_entry_t *)node->data;
    if (e && e->rd) return e->rd(node, off, len, buf);
    return -1;
}

static ssize_t dev_write(vfs_node_t *node, size_t off, size_t len, const void *buf) {
    devfs_entry_t *e = (devfs_entry_t *)node->data;
    if (e && e->wr) return e->wr(node, off, len, buf);
    return -1;
}

vfs_node_t *devfs_mount(void) {
    if (dev_root) return dev_root;
    dev_root = vfs_mkdir(vfs_root(), "dev");
    return dev_root;
}

int devfs_register_chr(const char *name, vfs_read_t rd, vfs_write_t wr, void *ctx) {
    if (!dev_root) dev_root = devfs_mount();
    vfs_node_t *n = vfs_create(dev_root, name, VFS_NODE_DEVICE);
    if (!n) return -1;
    devfs_entry_t *e = (devfs_entry_t *)kcalloc(1, sizeof(devfs_entry_t));
    if (!e) return -1;
    e->rd = rd;
    e->wr = wr;
    e->ctx = ctx;
    n->data = e;
    n->read = dev_read;
    n->write = dev_write;
    klog_printf(KLOG_INFO, "devfs: registered %s", name);
    return 0;
}

