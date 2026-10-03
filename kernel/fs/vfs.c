#include "fs/vfs.h"
#include "fs/fat32.h"
#include "mem.h"
#include <stdint.h>
#include <terminal/printf.h>

struct vfs_inode_ops* vfs_ops[4]; // 0 = FAT32, etc (That is the only supported filesystem)
struct vfs_filesystem* vfs[16];

struct vfs_filesystem* root_vfs;

void vfs_fat32_init() {
    if (vfs_ops[VFS_OPS_FAT32]) return;

    struct vfs_inode_ops* ops = (vfs_ops[VFS_OPS_FAT32] = kmalloc(sizeof(struct vfs_inode_ops)));
    ops->read = fat32_vfs_read_file;
    ops->lookup = fat32_vfs_lookup;
    ops->write = fat32_vfs_write_file;
}

void vfs_init() {
    memset(vfs, 0, sizeof(vfs));
    memset(vfs_ops, 0, sizeof(vfs_ops));
}
void vfs_add(struct drive_fs_t* fs) {
    int index = 0;
    while (vfs[index]) index++;

    struct vfs_filesystem* filesystem = (vfs[index] = kmalloc(sizeof(struct vfs_filesystem)));
    memcpy(filesystem->name, fs->volume_name, sizeof(fs->volume_name));

    filesystem->root = kmalloc(sizeof(struct vfs_inode));
    memset(filesystem->root, 0, sizeof(struct vfs_inode));

    filesystem->root->fs = filesystem;
    filesystem->root->mode = VFS_DIR;
    filesystem->root->ops = vfs_ops[VFS_OPS_FAT32]; // TODO: add support for more filesystems
    filesystem->root->ino = fs->root_dir.userdata2;
    filesystem->reserved = fs;

    root_vfs = filesystem;
}

// if inode is null, then this will use the root directory
int vfs_lookup(struct vfs_inode* inode, const char* name, struct vfs_inode* buffer) { return inode ? inode->ops->lookup(inode, name, buffer) : root_vfs->root->ops->lookup(root_vfs->root, name, buffer); }

int vfs_lookup_path(const char *name, struct vfs_inode *buffer) {
    if (!name || !buffer) return -1;

    char* tok = strtok((char*)name, "/");

    struct vfs_inode* search_in = name[0] == '/' ? memcpy(kmalloc(sizeof(struct vfs_inode)), root_vfs->root, sizeof(struct vfs_inode)) : NULL;
    if (!search_in) return -1;

    struct vfs_inode next;
    while (tok) {
        if (vfs_lookup(search_in, tok, &next) == -1) return -1;
        *search_in = next;

        tok = strtok(NULL, "/");
    }
    *buffer = *search_in;
    kfree(search_in);

    return 1;
}