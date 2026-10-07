#include "drivers/vga.h"
#include "fs/vfs.h"
#include "terminal/terminal.h"
#include "terminal/printf.h"
#include "sys/errno.h"
#include <fs/fat32.h>
#include <fs/fs.h>
#include <stddef.h>
#include <stdint.h>

struct drive_fs_t* fss[16];
int actual_fs = 0;

struct drive_fs_t *fs_drive_open( struct kdrive_t *drive, struct drive_fs_t* fs )
{
	struct partition_t part;

	part.type = FS_FAT32;
	part.lba = 0;
	part.size = SIZE_MAX;
	return fs_partition_open(drive, &part, fs);
}

struct drive_fs_t *fs_partition_open( struct kdrive_t *drive, struct partition_t *partition, struct drive_fs_t* fs )
{
	switch (partition->type)
	{
	case FS_FAT32:
        vfs_fat32_init();
        fs->userdata2 = (size_t)vfs_ops[VFS_OPS_FAT32];
		return fat32_drive_open(drive, partition);
	case FS_NONE:
	case FS_FAT12:
	case FS_FAT16:
		break;
	}
	return 0;
}

void fs_free_entries( struct fs_entries_t *entries )
{
	/* we do not have free lol, let that sink in */
}

int fsmount(int drive) {
    printc("\n", VGA_COLOR_WHITE);

    int i = 0;
    while (i < 16) {
        if (fss[i]) continue;
        break;
    }

    struct kdrive_t* d;
    if (!(d = get_kdrive(drive))) {
        // printc("No slave drive found. Is fat32.img attached as a second drive?\n", VGA_COLOR_RED);
        return -ENODEV;
    }
    fss[i]->drive = d;
    d->fs = fss[i] = fs_drive_open(d, fss[i]);
    if (!fss[i]) {
        set_printf_color(VGA_COLOR_RED);
            printf("Filesystem mount failed. %s is an invalid FAT32 drive\n", d->sysname);
        set_printf_color(VGA_COLOR_WHITE);
        return -1;
    }
    printf("Filesystem mounted successfully with %s.\n\n", d->sysname);
    actual_fs = i;
    memcpy(fss[i]->volume_name, d->sysname, sizeof(d->sysname));

    return 0;
}