#include "vfs.h"
#include "kmalloc.h"
#include "string.h"
#include "klog.h"
#include "initcall.h"

static vfs_node_t root_node;

static vfs_node_t *alloc_node(const char *name, vfs_node_type_t type) {
    vfs_node_t *n = (vfs_node_t *)kcalloc(1, sizeof(vfs_node_t));
    if (!n) return NULL;
    n->type = type;
    strcpy(n->name, name);
    return n;
}

int vfs_init(void) {
    root_node = (vfs_node_t){0};
    root_node.type = VFS_NODE_DIR;
    strcpy(root_node.name, "/");
    root_node.parent = &root_node;
    klog_printf(KLOG_INFO, "vfs: root ready");
    return 0;
}

vfs_node_t *vfs_root(void) { return &root_node; }

static void link_child(vfs_node_t *parent, vfs_node_t *child) {
    child->parent = parent;
    child->sibling = parent->child;
    parent->child = child;
}

vfs_node_t *vfs_create(vfs_node_t *parent, const char *name, vfs_node_type_t type) {
    if (!parent) parent = &root_node;
    vfs_node_t *n = alloc_node(name, type);
    if (!n) return NULL;
    link_child(parent, n);
    return n;
}

vfs_node_t *vfs_mkdir(vfs_node_t *parent, const char *name) {
    return vfs_create(parent, name, VFS_NODE_DIR);
}

static vfs_node_t *find_child(vfs_node_t *dir, const char *name) {
    for (vfs_node_t *c = dir->child; c; c = c->sibling) {
        if (strcmp(c->name, name) == 0) return c;
    }
    return NULL;
}

vfs_node_t *vfs_lookup(vfs_node_t *parent, const char *path) {
    if (!parent) parent = &root_node;
    if (!path || !*path) return parent;
    if (*path == '/') {
        parent = &root_node;
        ++path;
    }
    char segment[64];
    while (*path) {
        size_t i = 0;
        while (*path && *path != '/' && i + 1 < sizeof(segment)) segment[i++] = *path++;
        segment[i] = 0;
        if (i == 0) break;
        parent = find_child(parent, segment);
        if (!parent) return NULL;
        if (*path == '/') ++path;
    }
    return parent;
}

ssize_t vfs_read(vfs_node_t *node, size_t off, size_t len, void *buf) {
    if (!node || node->type == VFS_NODE_DIR) return -1;
    if (node->read) return node->read(node, off, len, buf);
    if (!node->data) return 0;
    if (off >= node->size) return 0;
    size_t avail = node->size - off;
    if (len > avail) len = avail;
    memcpy(buf, (char *)node->data + off, len);
    return (ssize_t)len;
}

ssize_t vfs_write(vfs_node_t *node, size_t off, size_t len, const void *buf) {
    if (!node || node->type == VFS_NODE_DIR) return -1;
    if (node->write) return node->write(node, off, len, buf);
    size_t need = off + len;
    if (need > node->size) {
        void *new_buf = krealloc(node->data, need);
        if (!new_buf) return -1;
        node->data = new_buf;
        node->size = need;
    }
    memcpy((char *)node->data + off, buf, len);
    return (ssize_t)len;
}

INITCALL_DEFINE(INITCALL_CORE, vfs_init);
