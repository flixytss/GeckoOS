#ifndef DRIVE_FS_H
#define DRIVE_FS_H

#include <mem.h>
#include <partition/partition.h>
#include <drivers/drives.h>
#include <stdint.h>
#define FILENAME_MAX 256

// nfoxers - forward declaration to fix -Wvisibility
struct drive_file_t;
struct drive_fs_t;
struct drive_dir_t;

typedef size_t (*fn_df_read)(struct drive_file_t *, size_t offset, size_t count, uint8_t *data);
typedef size_t (*fn_df_write)(struct drive_file_t *, size_t offset, size_t count, const uint8_t *data);
typedef size_t (*fn_df_truncate)(struct drive_file_t *, size_t size, const uint8_t *data);
typedef size_t (*fn_df_open)(struct drive_file_t *);
typedef size_t (*fn_df_close)(struct drive_file_t *);

typedef struct fs_entries_t (*fn_get_entries)(struct drive_dir_t *);

typedef struct fs_entries_t (*fn_root_get_entries)(struct drive_fs_t *);

typedef struct drive_dir_t *(*fn_get_root_entry)(struct drive_fs_t *);

enum drive_entry_type_t
{
	ENTRY_FILE = 2,
	ENTRY_DIRECTORY = 1,
	ENTRY_LINK = 3,
};

struct drive_file_t
{
	enum drive_entry_type_t type;
	char name[FILENAME_MAX];
	struct drive_fs_t *fs;
	size_t file_size;
	void *userdata1;
	size_t userdata2;
	size_t userdata3;
	size_t userdata4;
};

typedef struct fs_entries_t (*fs_get_dir_entries_t)(struct drive_dir_t*);

struct drive_dir_t
{
	enum drive_entry_type_t type;
	char name[FILENAME_MAX];
	struct drive_fs_t *fs;
	void *userdata1;
	size_t userdata2;
	size_t userdata3;
	size_t userdata4;
	fs_get_dir_entries_t get_entries;
};

typedef union drive_entry_t
{
	enum drive_entry_type_t type;
	struct drive_dir_t dir;
	struct drive_file_t file;
} drive_entry_t;

struct fs_entries_t
{
	size_t count;
	drive_entry_t *entries;
};

struct drive_fs_t
{
	char volume_name[32];
	struct kdrive_t *drive;
	void *userdata1;
	size_t userdata2;
	struct drive_dir_t root_dir;
};

struct drive_fs_t *fs_drive_open( struct kdrive_t *drive, struct drive_fs_t* fs );
struct drive_fs_t *fs_partition_open( struct kdrive_t *drive, struct partition_t *partition, struct drive_fs_t* fs );
void fs_free_entries( struct fs_entries_t *entries );

Buffer_t readfile(unsigned char* fname);
int fsmount(int drive);

extern struct drive_fs_t* fss[16];
extern int actual_fs;

#endif
