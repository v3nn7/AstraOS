#pragma once

#include "types.h"

typedef enum {
    VFS_NODE_FILE = 0,
    VFS_NODE_DIR  = 1,
    VFS_NODE_DEVICE = 2
} vfs_node_type_t;

struct vfs_node;

typedef ssize_t (*vfs_read_t)(struct vfs_node *, size_t, size_t, void *);
typedef ssize_t (*vfs_write_t)(struct vfs_node *, size_t, size_t, const void *);
typedef int (*vfs_ioctl_t)(struct vfs_node *, int cmd, void *arg);

typedef struct vfs_node {
    char name[64];
    vfs_node_type_t type;
    struct vfs_node *parent;
    struct vfs_node *child;
    struct vfs_node *sibling;
    vfs_read_t read;
    vfs_write_t write;
    vfs_ioctl_t ioctl;
    void *data;
    size_t size;
} vfs_node_t;

int vfs_init(void);
vfs_node_t *vfs_root(void);
vfs_node_t *vfs_mkdir(vfs_node_t *parent, const char *name);
vfs_node_t *vfs_create(vfs_node_t *parent, const char *name, vfs_node_type_t type);
vfs_node_t *vfs_lookup(vfs_node_t *parent, const char *path);
ssize_t vfs_read(vfs_node_t *node, size_t off, size_t len, void *buf);
ssize_t vfs_write(vfs_node_t *node, size_t off, size_t len, const void *buf);

/* DevFS */
vfs_node_t *devfs_mount(void);
int devfs_register_chr(const char *name, vfs_read_t rd, vfs_write_t wr, void *ctx);

/* RamFS */
vfs_node_t *ramfs_mount(void);

/* Initrd */
void initrd_load(void);
