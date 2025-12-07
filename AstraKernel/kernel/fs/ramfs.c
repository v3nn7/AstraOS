#include "vfs.h"
#include "klog.h"

static vfs_node_t *ram_root = NULL;

vfs_node_t *ramfs_mount(void) {
    if (ram_root) return ram_root;
    ram_root = vfs_mkdir(vfs_root(), "ramfs");
    klog_printf(KLOG_INFO, "ramfs: mounted");
    return ram_root;
}

