#pragma once

#include <stddef.h>
#include <stdint.h>
#include "fs/fs.h"

#define PATH_MAX 4096

// VFS options
#define VFS_OPS_FAT32 0
#define VFS_OPS_DEVFS 1

#define VFS_DIR                 1 // just a directory
#define VFS_FILE                2 // just a file
#define VFS_CHARACTER_DEVICE    3 // a character file like /dev/null
#define VFS_BLOCK_DEVICE        4 // a block device (just a drive as a file)

struct vfs_inode {
    uint64_t ino;
    uint32_t mode;
    uint64_t size;

    struct vfs_filesystem* fs;
    struct vfs_inode_ops* ops;
    struct mount* mount;

    void* reserved;

    struct vfs_inode* next;
};

struct mount {
    struct vfs_inode* mountpoint;
    struct vfs_inode* root;
    struct vfs_filesystem* fs;
};

struct vfs_inode_ops {
    int (*lookup)(struct vfs_inode* inode, const char* name, struct vfs_inode** buffer);
    size_t (*read)(struct vfs_inode* inode, size_t, uint8_t* buffer, size_t count);
    size_t (*write)(struct vfs_inode* inode, size_t offset, const uint8_t* buffer, size_t count);
    int (*mkdir)(struct vfs_inode* inode, const char* name);
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

struct dentry {
    struct vfs_inode* inode;
    char path[4096];
};

extern struct vfs_filesystem vfs_ramfs;

struct vfs_inode* vfs_inode_create(struct vfs_filesystem* fs, uint64_t ino);

void vfs_add(struct drive_fs_t* fs);

void vfs_fat32_init();
void vfs_devfs_init();

int vfs_lookup(struct vfs_inode* inode, const char* name, struct vfs_inode** buffer);
int vfs_lookup_path(char *name, struct vfs_inode** buffer);
size_t vfs_read(struct vfs_inode* inode, uint8_t* buffer, size_t count, size_t offset);
size_t vfs_write(struct vfs_inode* inode, uint8_t* buffer, size_t count, size_t offset);
void vfs_set_root_(struct vfs_filesystem* vfs);
struct vfs_filesystem* get_vfs_(int index);
int vfs_mount(const char* name, struct vfs_filesystem* vfs);
int vfs_umount(const char* name);
void vfs_add_vfs(struct vfs_filesystem* vfs);
int vfs_mkdir(struct vfs_inode* inode, const char* name);
int vfs_mkdir_path(const char* name);

// VFS Options
extern struct vfs_inode_ops* vfs_ops[4];