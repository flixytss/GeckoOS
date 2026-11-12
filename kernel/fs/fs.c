#include "drivers/vga.h"
#include "fs/vfs.h"
#include "mem/mmu.h"
#include "partition/partition.h"
#include "sys/std.h"
#include "terminal/terminal.h"
#include "terminal/printf.h"
#include "sys/errno.h"
#include "fs/types/fat32.h"
#include "fs/fs.h"
#include "sys/types.h"

struct drive_fs_t* fss[16];
int actual_fs = 0;

struct drive_fs_t *fs_partition_open(struct kdrive_t *drive, struct partition_t *partition)
{
	switch (partition->type) {
        case 0xc: // FAT32
        case 0xb:
            vfs_fat32_init();
            return fat32_drive_open(drive, partition);
        default: break;
	}
	return 0;
}

void fs_free_entries(struct fs_entries_t *entries) {
	
}

int fsmount(int drive) {
    int i = 0;
    while (i < 16) {
        if (fss[i++]) continue;
        break;
    }

    struct kdrive_t* d;
    if (!(d = get_kdrive(drive))) {
        // printc("No slave drive found. Is fat32.img attached as a second drive?\n", VGA_COLOR_RED);
        return -ENODEV;
    }
    d->fs = fss[i] = fs_partition_open(d, d->partitions.partitions);
    if (!fss[i]) {
        set_printf_color(VGA_COLOR_RED);
            printf("Filesystem mount failed. %s is an invalid FAT32 drive\n", d->sysname);
        set_printf_color(VGA_COLOR_WHITE);

        kfree(d->fs->drive->partitions.partitions);
        return -1;
    }
    printf("Filesystem mounted successfully with %s:%s.\n\n", d->sysname, d->partitions.partitions->sysname ? d->partitions.partitions->sysname : "N/A");
    memcpy(fss[i]->volume_name, d->sysname, sizeof(d->sysname));
    actual_fs = i;

    return 0;
}