#ifndef MBR_H
#define MBR_H

#include <partition/partition.h>

struct mbr_partition_t
{
	u8 flags;
	u32 chr_start: 24;
	u8 type;
	u32 chr_end: 24;
	u32 lba;
	u32 size;
};

typedef struct mbr_partition_t mbr_t[4];

#endif
