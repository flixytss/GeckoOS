#ifndef PARTITION_H
#define PARTITION_H

#include "sys/types.h"

enum partition_fs_t
{
	FS_NONE,
	FS_FAT12,
	FS_FAT16,
	FS_FAT32,
};

struct partition_t
{
	enum partition_fs_t type;
	u64 lba;
	u64 size;
};

enum partition_type_t
{
	PARTITION_NONE,
	PARTITION_MBR,
	PARTITION_GPT,
};

struct partition_table_t
{
	enum partition_type_t type;
	u8 count;
	struct partition_t *partitions;
};

#endif
