#pragma once

#include "fs/vfs.h"

struct vfs_filesystem* create_devfs();

size_t devfs_vfs_read(struct vfs_inode *inode, size_t offset, uint8_t *buffer, size_t count);
size_t devfs_vfs_write(struct vfs_inode *inode, size_t offset, const uint8_t *content, size_t len);
int devfs_vfs_lookup(struct vfs_inode *inode, const char *name, struct vfs_inode **buffer);
int devfs_vfs_mkdir(struct vfs_inode *inode, const char *name);

#ifdef DEBUG
    void devfs_vfs_print_node_childrens(struct vfs_inode* inode);
#endif