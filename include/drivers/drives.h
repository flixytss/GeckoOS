#ifndef DRIVES_H
#define DRIVES_H

#include "sys/types.h"
#include <mem.h>
#include <sys/types.h>
#include <partition/partition.h>

struct kdrive_t;

typedef ssize_t (*kdrive_read_sectors)(struct kdrive_t *drive, u64 lba, size_t count, u8 *buf);
typedef ssize_t (*kdrive_write_sectors)(struct kdrive_t *drive, u64 lba, size_t count, const u8 *buf);

struct kdrive_t
{
	char name[64];
	char sysname[16];

	u32 sector_size;
	u64 userdata1;
	u32 userdata2;

	struct partition_table_t partitions;
	kdrive_read_sectors read;
	kdrive_write_sectors write;

	struct drive_fs_t* fs;
};

void register_kdrive( struct kdrive_t drive );
struct kdrive_t *get_kdrive( int i );

extern struct kdrive_t drives[32];

#endif
