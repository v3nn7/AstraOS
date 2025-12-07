/**
 * Helper function to load cursor PNG into ramfs
 * This can be called during kernel initialization to populate /assets/ with cursor.png
 */
#include "vfs.h"
#include "drivers.h"
#include "kmalloc.h"
#include "string.h"
#include "kernel.h"

/**
 * Create /assets directory and load cursor.png from embedded data (if available)
 * This allows users to place cursor.png in initrd/ramfs without recompiling kernel
 */
void cursor_setup_assets(void) {
    /* Create /assets directory */
    vfs_node_t *assets_dir = vfs_lookup(NULL, "/assets");
    if (!assets_dir) {
        assets_dir = vfs_mkdir(vfs_root(), "assets");
        if (assets_dir) {
            printf("cursor: created /assets directory\n");
        }
    }
    
    /* If cursor.png doesn't exist in /assets, try to create it from embedded data */
    vfs_node_t *cursor_file = vfs_lookup(NULL, "/assets/cursor.png");
    if (!cursor_file) {
        #ifdef CURSOR_EMBEDDED
        extern const unsigned char cursor_png[];
        extern const unsigned int cursor_png_len;
        
        /* Create cursor.png file in ramfs */
        cursor_file = vfs_create(assets_dir, "cursor.png", VFS_NODE_FILE);
        if (cursor_file) {
            cursor_file->data = kmalloc(cursor_png_len);
            if (cursor_file->data) {
                memcpy(cursor_file->data, cursor_png, cursor_png_len);
                cursor_file->size = cursor_png_len;
                printf("cursor: created /assets/cursor.png from embedded data (%u bytes)\n", cursor_png_len);
            }
        }
        #endif
    } else {
        printf("cursor: /assets/cursor.png already exists (%zu bytes)\n", cursor_file->size);
    }
}

