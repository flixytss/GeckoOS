#pragma once

#include <stddef.h>
#include <stdint.h>
#include "fs/fs.h"

// VFS options
#define VFS_OPS_FAT32 0

#define VFS_DIR  1
#define VFS_FILE 2

struct vfs_inode {
    uint64_t ino;
    uint32_t mode;
    uint64_t size;

    struct vfs_filesystem* fs;
    struct vfs_inode_ops* ops;

    void* reserved;
};

struct vfs_inode_ops {
    int (*lookup)(struct vfs_inode* inode, const char* name, struct vfs_inode* buffer);
    int (*read)(struct vfs_inode* inode, size_t, uint8_t* buffer, size_t count);
    int (*write)(struct vfs_inode* inode, size_t offset, const uint8_t* buffer, size_t count);
};

struct vfs_filesystem {
    char name[64];

    struct vfs_inode* root;

    void* reserved;
};

struct vfs_file {
    struct vfs_inode* inode;
    uint32_t offset;
    uint32_t flags;
};

// ramfs

struct ramfs_node {
    struct vfs_inode* inode;

    char* name;

    struct ramfs_node* parent;
    struct ramfs_node** children;
    int children_count;

    uint8_t* data;
};

extern struct vfs_filesystem vfs_ramfs;

void vfs_add(struct drive_fs_t* fs);
void vfs_init();

void vfs_fat32_init();

int vfs_lookup(struct vfs_inode* inode, const char* name, struct vfs_inode* buffer);
int vfs_lookup_path(const char *name, struct vfs_inode *buffer);

// VFS Options
extern struct vfs_inode_ops* vfs_ops[4];

extern struct vfs_filesystem* root_vfs;