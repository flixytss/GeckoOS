#pragma once

#include "sys/types.h"
#include "fs/vfs.h"

size_t fat32_vfs_read_file(struct vfs_inode* inode, size_t offset, u8* buffer, size_t count);

int fat32_vfs_lookup(struct vfs_inode* inode, const char* name, struct vfs_inode** buffer);

size_t fat32_vfs_write_file(struct vfs_inode* inode, size_t offset, const u8 *content, size_t len);

int fat32_vfs_mkdir(struct vfs_inode* inode, const char* name);

int fat32_vfs_create(struct vfs_inode* inode, const char* name);