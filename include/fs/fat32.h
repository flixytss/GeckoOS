//Ember2819
#ifndef FAT32_H
#define FAT32_H

#include <stdint.h>
#include <partition/partition.h>
#include <drivers/drives.h>
#include <fs/fs.h>
#include "fs/vfs.h"

struct drive_fs_t *fat32_drive_open( struct kdrive_t *drive, struct partition_t *partition );
struct drive_fs_t *fat32_drive_close( struct drive_fs_t *fs );

int fat32_create_file( struct drive_fs_t *fs, char *name,
                       const uint8_t *content, size_t len );

int fat32_delete_file( struct drive_fs_t *fs, char *name );

int fat32_append_file( struct drive_fs_t *fs, char *name,
                       const uint8_t *content, size_t len );

int fat32_mkdir(struct drive_fs_t *fs, uint32_t dir_cluster, const char *name);

void fat32_print_info( struct drive_fs_t *fs, uint8_t color );

size_t fat32_vfs_read_file(struct vfs_inode* inode, size_t offset, uint8_t* buffer, size_t count);

int fat32_vfs_lookup(struct vfs_inode* inode, const char* name, struct vfs_inode** buffer);

size_t fat32_vfs_write_file(struct vfs_inode* inode, size_t offset, const uint8_t *content, size_t len);

int fat32_vfs_mkdir(struct vfs_inode* inode, const char* name);

#endif // FAT32_H
