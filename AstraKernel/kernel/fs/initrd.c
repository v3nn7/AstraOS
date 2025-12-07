/*
 * Initrd CPIO parser
 * Based on OSDev Wiki CPIO format specification
 */

#include "vfs.h"
#include "kmalloc.h"
#include "string.h"
#include "kernel.h"
#include "limine.h"
#include "memory.h"
#include "vmm.h"

/* HHDM offset for physical to virtual conversion */
extern uint64_t pmm_hhdm_offset;

/* CPIO newc format magic */
#define CPIO_MAGIC_NEWC "070701"
#define CPIO_TRAILER "TRAILER!!!"

/* Limine module request - declared in boot/limine_requests.c */
extern volatile struct limine_module_request limine_module_request;

/* Helper: hex string to number */
static uint32_t hex_to_uint32(const char *hex) {
    uint32_t val = 0;
    for (int i = 0; i < 8 && hex[i]; i++) {
        char c = hex[i];
        val <<= 4;
        if (c >= '0' && c <= '9') val |= (c - '0');
        else if (c >= 'a' && c <= 'f') val |= (c - 'a' + 10);
        else if (c >= 'A' && c <= 'F') val |= (c - 'A' + 10);
    }
    return val;
}

/* Parse CPIO header */
static bool parse_cpio_header(const uint8_t *data, size_t *offset, size_t max_size, char *filename, size_t *filesize, uint32_t *mode) {
    if (!data || *offset + 110 > max_size) return false; /* Sanity check */
    
    /* Check magic - ensure we don't read beyond bounds */
    if (*offset + 6 > max_size) return false;
    if (memcmp((char *)data + *offset, CPIO_MAGIC_NEWC, 6) != 0) {
        return false;
    }
    
    /* Parse header fields (all hex strings) */
    char hex_buf[9] = {0};
    
    /* Read filesize (offset 54, 8 bytes) */
    memcpy(hex_buf, (char *)data + *offset + 54, 8);
    *filesize = (size_t)hex_to_uint32(hex_buf);
    
    /* Read namesize (offset 110, 8 bytes) */
    memcpy(hex_buf, (char *)data + *offset + 110, 8);
    size_t namesize = (size_t)hex_to_uint32(hex_buf);
    
    /* Read mode (offset 14, 8 bytes) */
    memcpy(hex_buf, (char *)data + *offset + 14, 8);
    *mode = hex_to_uint32(hex_buf);
    
    /* Skip header (110 bytes) */
    *offset += 110;
    
    /* Read filename - ensure we don't read beyond bounds */
    if (*offset + namesize > max_size) {
        printf("initrd: namesize %zu exceeds remaining data\n", namesize);
        return false;
    }
    if (namesize > 256) namesize = 256; /* Safety limit */
    if (namesize > 1) {
        memcpy(filename, (char *)data + *offset, namesize - 1);
        filename[namesize - 1] = '\0';
    } else {
        filename[0] = '\0';
    }
    
    /* Align to 4 bytes */
    *offset += namesize;
    *offset = (*offset + 3) & ~3;
    
    return true;
}

/* Load initrd into ramfs */
void initrd_load(void) {
    struct limine_module_response *modules = limine_module_request.response;
    
    if (!modules || modules->module_count == 0) {
        printf("initrd: no modules found\n");
        return;
    }
    
    printf("initrd: found %zu module(s)\n", modules->module_count);
    
    /* Process each module (initrd is typically the first one) */
    for (size_t i = 0; i < modules->module_count; i++) {
        struct limine_file *module = modules->modules[i];
        
        if (!module) {
            printf("initrd: module %zu is NULL, skipping\n", i);
            continue;
        }
        
        /* Safely read path - it may be NULL */
        const char *path_str = "(null)";
        if (module->path) {
            /* Check if path pointer is valid (within reasonable range) */
            uint64_t path_addr = (uint64_t)module->path;
            if (path_addr >= 0x1000 && path_addr < 0x100000000ULL) {
                path_str = module->path;
            }
        }
        
        printf("initrd: module %zu: path='%s' size=%zu\n", i, path_str, module->size);
        
        if (module->size == 0) {
            printf("initrd: module %zu is empty, skipping\n", i);
            continue;
        }
        
        /* Convert physical address to HHDM virtual address */
        /* module->address is physical address from Limine */
        uint64_t phys_addr = (uint64_t)module->address;
        
        /* Validate physical address */
        if (phys_addr == 0 || phys_addr >= pmm_max_physical) {
            printf("initrd: module %zu has invalid physical address %p, skipping\n",
                   i, (void *)phys_addr);
            continue;
        }
        
        uint64_t virt_addr = pmm_hhdm_offset + phys_addr;
        
        printf("initrd: module %zu: phys=%p virt=%p hhdm_offset=%p\n",
               i, (void *)phys_addr, (void *)virt_addr, (void *)pmm_hhdm_offset);
        
        /* Ensure the module pages are mapped in HHDM */
        uint64_t module_end = phys_addr + module->size;
        uint64_t page_start = phys_addr & ~0xFFFULL;
        uint64_t page_end = (module_end + 0xFFF) & ~0xFFFULL;
        
        printf("initrd: module %zu: mapping pages %p-%p (%zu pages)\n",
               i, (void *)page_start, (void *)page_end, (page_end - page_start) / 4096);
        
        /* Map all pages of the module into HHDM */
        for (uint64_t page = page_start; page < page_end; page += 4096) {
            uint64_t page_virt = pmm_hhdm_offset + page;
            vmm_map(page_virt, page, PAGE_PRESENT | PAGE_WRITE);
        }
        
        printf("initrd: module %zu: pages mapped, reading CPIO data\n", i);
        
        /* Now we can safely access the data via HHDM */
        /* Now we can safely access the data via HHDM */
        const uint8_t *cpio_data = (const uint8_t *)virt_addr;
        size_t offset = 0;
        size_t total_size = module->size;
        
        printf("initrd: parsing CPIO archive (size=%zu) at virt=%p\n", total_size, (void *)virt_addr);
        
        /* Parse CPIO archive */
        while (offset < total_size && offset + 110 < total_size) {
            char filename[256] = {0};
            size_t filesize = 0;
            uint32_t mode = 0;
            
            if (!parse_cpio_header(cpio_data, &offset, total_size, filename, &filesize, &mode)) {
                printf("initrd: failed to parse header at offset %zu\n", offset);
                break;
            }
            
            /* Check for trailer */
            if (strcmp(filename, CPIO_TRAILER) == 0) {
                printf("initrd: reached end of archive\n");
                break;
            }
            
            printf("initrd: entry: '%s' size=%zu mode=0x%x\n", filename, filesize, mode);
            
            /* Check if it's a directory */
            if ((mode & 0170000) == 0040000) {
                /* Directory */
                vfs_node_t *dir = vfs_lookup(NULL, filename);
                if (!dir) {
                    /* Create directory path */
                    char path_buf[256];
                    strcpy(path_buf, filename);
                    char *slash = path_buf;
                    
                    /* Skip leading slash */
                    if (*slash == '/') slash++;
                    
                    vfs_node_t *current = vfs_root();
                    
                    /* Create each directory component */
                    while (*slash) {
                        char *next_slash = strchr(slash, '/');
                        if (next_slash) *next_slash = '\0';
                        
                        vfs_node_t *child = vfs_lookup(current, slash);
                        if (!child) {
                            child = vfs_mkdir(current, slash);
                            if (child) {
                                printf("initrd: created directory '%s'\n", slash);
                            }
                        }
                        
                        if (next_slash) {
                            *next_slash = '/';
                            slash = next_slash + 1;
                            current = child;
                        } else {
                            break;
                        }
                    }
                }
            } else {
                /* Regular file */
                if (offset + filesize > total_size) {
                    printf("initrd: file '%s' extends beyond archive, skipping\n", filename);
                    break;
                }
                
                /* Find parent directory */
                char *last_slash = strrchr(filename, '/');
                vfs_node_t *parent = NULL;
                char *basename = filename;
                
                if (last_slash) {
                    *last_slash = '\0';
                    parent = vfs_lookup(NULL, filename);
                    basename = last_slash + 1;
                    *last_slash = '/';
                } else {
                    parent = vfs_root();
                }
                
                if (!parent) {
                    printf("initrd: parent directory not found for '%s', skipping\n", filename);
                    offset = (offset + filesize + 3) & ~3; /* Skip file data */
                    continue;
                }
                
                /* Create file node */
                vfs_node_t *file = vfs_create(parent, basename, VFS_NODE_FILE);
                if (file) {
                    /* Allocate memory for file data */
                    file->data = kmalloc(filesize);
                    if (file->data) {
                        memcpy(file->data, cpio_data + offset, filesize);
                        file->size = filesize;
                        printf("initrd: loaded file '%s' (%zu bytes)\n", filename, filesize);
                    } else {
                        printf("initrd: failed to allocate memory for '%s'\n", filename);
                    }
                } else {
                    printf("initrd: failed to create file node '%s'\n", filename);
                }
                
                /* Skip file data (aligned to 4 bytes) */
                offset = (offset + filesize + 3) & ~3;
            }
        }
        
        printf("initrd: module %zu processed\n", i);
    }
    
    printf("initrd: loading complete\n");
}
