#ifndef PARTITION_H
#define PARTITION_H

#include "sys/types.h"

struct partition_t
{
	char* sysname;
	u8 type;
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
	u8 count;
	enum partition_type_t type;
	struct partition_t *partitions;
};

#endif
