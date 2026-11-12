#include "partition/partition.h"
#include <drivers/drives.h>
#include <mem/mmu.h>
#include <sys/std.h>
#include <terminal/terminal.h>
#include <terminal/printf.h>
#include <partition/mbr.h>

struct kdrive_t* drives[32];

static void load_parameters( struct kdrive_t *drive )
{
	char partition_name[16];
	u8 sector[512];
	struct mbr_partition_t mbr[4];
	int i;

	drive->read((void*)drive, 0, 1, sector);

	if (sector[510] == 0x55 && sector[511] == 0xaa) {
		memcpy(mbr, sector+446, sizeof(struct mbr_partition_t) * 4);

		drive->partitions.count = 0;
		drive->partitions.type = PARTITION_MBR;
		drive->partitions.partitions = kmalloc(4 * sizeof(struct partition_t));

		for (i = 0; i < 4; i++) {
			if (!mbr[i].type) continue;

			npf_snprintf(partition_name, 16, "part%x", i);

			drive->partitions.partitions[i].sysname = strdup(partition_name);
			drive->partitions.partitions[i].type = mbr[i].type;
			drive->partitions.partitions[i].lba = mbr[i].lba;
			drive->partitions.partitions[i].size = mbr[i].size;
			drive->partitions.count++;

			#ifdef DEBUG
				printf("      %d: found partition with size: %d bytes, lba: 0x%llx, type: 0x%x\n", i, mbr[i].size * 512, drive->partitions.partitions[i].lba, mbr[i].type);
				printf("        %d: partition sysname: %s\n", i, drive->partitions.partitions[i].sysname);
			#endif
		}

		/*
			If the disk dosen't have partitions, make
			a partition that uses all the drive
		*/
		if (drive->partitions.count == 0) {
			strncpy(partition_name, "whole", 16);
			drive->partitions.partitions[0].sysname = strdup(partition_name);
			drive->partitions.partitions[0].type = 0xC; // FAT32 as default
			drive->partitions.partitions[0].lba = 0;
			drive->partitions.partitions[0].size = drive->size_in_sectors;
			drive->partitions.count++;
		}
	}
	else
	{
		/*
			Is the drive isn't MBR, We are gonna
			interpret the drive as a raw image
		*/

		drive->partitions.count = 1;
		drive->partitions.type = PARTITION_NONE;
		drive->partitions.partitions = kmalloc(sizeof(struct partition_t));
		drive->partitions.partitions[0].type = 0;
		drive->partitions.partitions[0].lba  = 0;
		drive->partitions.partitions[0].size = 0;
	}
};

void register_kdrive(struct kdrive_t* drive) {
	int i;
	for ( i = 0; i < 32; i++ ) {
		if (drives[i]) continue;
		load_parameters(drive);
		drives[i] = drive;
		npf_snprintf(drive->sysname, sizeof(drive->sysname), "gkd%d", i); // gkd = geckos drive

		break;
	}
}

struct kdrive_t *get_kdrive( int i )
	{ return drives[i]; }