//Ember2819
#ifndef FAT32_H
#define FAT32_H

#include "sys/types.h"
#include <partition/partition.h>
#include <drivers/drives.h>
#include <fs/fs.h>

typedef struct __attribute__((packed)) {
    u8  jmp_boot[3];
    u8  oem_name[8];
    u16 bytes_per_sector;
    u8  sectors_per_cluster;
    u16 reserved_sectors;
    u8  num_fats;
    u16 root_entry_count;   // Must be 0 for FAT32
    u16 total_sectors_16;   // Must be 0 for FAT32
    u8  media_type;
    u16 sectors_per_fat_16; // Must be 0 for FAT32
    u16 sectors_per_track;
    u16 num_heads;
    u32 hidden_sectors;
    u32 total_sectors_32;

    // FAT32 extended fields
    u32 sectors_per_fat_32;
    u16 ext_flags;
    u16 fs_version;
    u32 root_cluster;       // Usually 2
    u16 fs_info_sector;
    u16 backup_boot_sector;
    u8  reserved[12];
    u8  drive_number;
    u8  reserved1;
    u8  boot_signature;
    u32 volume_id;
    u8  volume_label[11];
    u8  fs_type[8];         // "FAT32   "
} FAT32_BPB;

typedef struct {
    FAT32_BPB bpb;

    u32  partition_lba;
    u32  fat_lba;
    u32  data_lba;       // FAT32 has no fixed root-dir region
    u32  root_cluster;   // First cluster of root directory
    u32  total_sectors;
    u32  sectors_per_fat;
    u8   mounted;
    struct kdrive_t *drive;
} FAT32_Volume;

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
u32 fat32_next_cluster(struct kdrive_t *drive, FAT32_Volume *pvol,
                                   u32 cluster);

#endif // FAT32_H
