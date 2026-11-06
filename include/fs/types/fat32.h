//Ember2819
#ifndef FAT32_H
#define FAT32_H

#include "sys/types.h"
#include <partition/partition.h>
#include <drivers/drives.h>
#include <fs/fs.h>

struct drive_fs_t *fat32_drive_open( struct kdrive_t *drive, struct partition_t *partition );
struct drive_fs_t *fat32_drive_close( struct drive_fs_t *fs );

int fat32_create_file( struct drive_fs_t *fs, char *name,
                       const u8 *content, size_t len );

int fat32_delete_file( struct drive_fs_t *fs, char *name );

// int fat32_append_file( struct drive_fs_t *fs, char *name,
//                        const u8 *content, size_t len );

struct fs_entries_t fat32_get_dir_entries(struct drive_fs_t *fs, u32 dir_cluster);

int fat32_mkdir(struct drive_fs_t *fs, u32 dir_cluster, const char *name);

void fat32_print_info( struct drive_fs_t *fs, u8 color );

size_t fat32_write_file(struct drive_file_t* file, size_t offset, const u8 *content, size_t len);
size_t fat32_file_read(struct drive_file_t* file, u32 offset, size_t len, u8 *buf);

#endif // FAT32_H
