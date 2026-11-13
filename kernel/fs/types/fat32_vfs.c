#include "fs/fs.h"
#include "fs/vfs.h"
#include "fs/types/fat32.h"
#include "sys/errno.h"
#include "sys/std.h"
#include "mem/mmu.h"

size_t fat32_vfs_write_file(struct vfs_inode* inode, size_t offset, const u8 *content, size_t len)
    { return fat32_write_file(inode->reserved, offset, content, len); }

size_t fat32_vfs_read_file(struct vfs_inode* inode, size_t offset, u8* buffer, size_t count)
    { return fat32_file_read(inode->reserved, offset, count, buffer); }

int fat32_vfs_mkdir(struct vfs_inode* inode, const char* name) 
    { return fat32_mkdir(inode->fs->reserved, inode->ino, name); }

int fat32_vfs_create(struct vfs_inode* inode, const char* name)
    { return fat32_create_file(inode->fs->reserved, (char*)name, (const u8*)"", 0); }

int fat32_vfs_lookup(struct vfs_inode* inode, const char* name, struct vfs_inode** buffer) {
    struct fs_entries_t entries = fat32_get_dir_entries(inode->fs->reserved, inode->ino);

    for (int i = 0; i < entries.count; i++) {
        if (strcasecmp(name, entries.entries[i].file.name) != 0) continue;

        u64 ino = 0;

        if (entries.entries[i].type == VFS_FILE)
            ino = entries.entries[i].file.userdata2;
        else
            ino = entries.entries[i].dir.userdata2;

        struct vfs_inode* find = vfs_inode_create(inode->fs, ino);
        if (!find) {
            kfree(entries.entries);
            return -ENOMEM;
        }

        if (entries.entries[i].type == VFS_FILE) find->size = entries.entries[i].file.file_size;
        find->mode = entries.entries[i].type;
        find->reserved = memcpy(kmalloc(sizeof(drive_entry_t)), &entries.entries[i], sizeof(drive_entry_t)); // TODO: Add memory checker
        find->ops = vfs_ops[VFS_OPS_FAT32];

        *buffer = find;
        kfree(entries.entries);
        return 0;
    }

    kfree(entries.entries);
    return -ENOENT;
}

struct vfs_dir* fat32_vfs_opendir(struct vfs_inode* inode) {
    struct vfs_dir* dir = kmalloc(sizeof(struct vfs_dir));

    u32 son_cluster = fat32_next_cluster(((struct drive_fs_t*)inode->fs->reserved)->drive, ((struct drive_file_t*)inode->reserved)->userdata1, inode->ino);

    dir->inode = inode;
    dir->current = vfs_inode_create(inode->fs, son_cluster);

    return dir;
}