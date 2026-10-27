#pragma once

#include "fs/vfs.h"

struct devfs_node {
    struct vfs_inode* inode;

    char* name;
    struct device* device;

    struct devfs_node* childrens; // this is a linked list with next
    struct devfs_node* next;
    // struct ramfs_node* last;
};

struct device {
    char* name;

    size_t (*read)(struct devfs_node* devfs, u8* buffer, size_t len);
    size_t (*write)(struct devfs_node* devfs, const u8* buffer, size_t len);

    void* reserved;
};

struct vfs_filesystem* create_devfs();

size_t devfs_vfs_read(struct vfs_inode* inode, size_t offset, u8* buffer, size_t count);
size_t devfs_vfs_write(struct vfs_inode* inode, size_t offset, const u8* content, size_t len);
int devfs_vfs_lookup(struct vfs_inode* inode, const char* path, struct vfs_inode** buffer);
int devfs_vfs_mkdir(struct vfs_inode* inode, const char* path);

#ifdef DEBUG
    void devfs_vfs_print_node_childrens(struct vfs_inode* inode);
    void devfs_vfs_print_recursive_nodes(struct vfs_inode* inode);
#endif
