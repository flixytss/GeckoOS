#include "fs/vfs.h"
#include "fs/fat32.h"
#include "fs/devfs.h"
#include "mem.h"
#include <stddef.h>
#include <stdint.h>
#include <terminal/printf.h>
#include "sys/errno.h"

struct vfs_inode_ops* vfs_ops[4]; // 0 = FAT32, 1 = devfs
struct vfs_filesystem* vfs[16];

struct vfs_filesystem* root_vfs;

static struct vfs_inode* vfs_cache;

struct vfs_inode* vfs_inode_find(struct vfs_filesystem* fs, uint64_t ino) {
    struct vfs_inode* curl = vfs_cache;

    while (curl) {
        if (curl->fs == fs && curl->ino == ino) return curl;

        curl = curl->next;
    }

    return NULL;
}
struct vfs_inode* vfs_inode_create(struct vfs_filesystem* fs, uint64_t ino) {
    if (!fs) return NULL;

    struct vfs_inode* inode;
    if ((inode = vfs_inode_find(fs, ino))) return inode;

    inode = kmalloc(sizeof(struct vfs_inode));
    if (!inode) return NULL;

    memset(inode, 0, sizeof(struct vfs_inode));

    inode->fs = fs;
    inode->ino = ino;

    inode->next = vfs_cache;
    vfs_cache = inode;

    return inode;
}

void vfs_fat32_init() {
    if (vfs_ops[VFS_OPS_FAT32]) return;

    struct vfs_inode_ops* ops = (vfs_ops[VFS_OPS_FAT32] = kmalloc(sizeof(struct vfs_inode_ops)));
    ops->read = fat32_vfs_read_file;
    ops->lookup = fat32_vfs_lookup;
    ops->write = fat32_vfs_write_file;
    ops->mkdir = fat32_vfs_mkdir;
}
void vfs_devfs_init() {
    if (vfs_ops[VFS_OPS_DEVFS]) return;

    struct vfs_inode_ops* ops = (vfs_ops[VFS_OPS_DEVFS] = kmalloc(sizeof(struct vfs_inode_ops)));
    ops->read = devfs_vfs_read;
    ops->lookup = devfs_vfs_lookup;
    ops->write = devfs_vfs_write;
    ops->mkdir = devfs_vfs_mkdir;
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
}
void vfs_add_vfs(struct vfs_filesystem* fs) {
    int index = 0;
    while (vfs[index]) index++;

    vfs[index] = fs;
}

struct vfs_filesystem* get_vfs_(int index) { return vfs[index]; }
void vfs_set_root_(struct vfs_filesystem* vfs) { root_vfs = vfs; }

// if inode is null, then this will use the root directory
int vfs_lookup(struct vfs_inode* inode, const char* name, struct vfs_inode** buffer) { return inode ? (inode->mode == VFS_DIR ? inode->ops->lookup(inode, name, buffer) : -ENOTDIR) : root_vfs->root->ops->lookup(root_vfs->root, name, buffer); }

int vfs_lookup_path(char *name, struct vfs_inode** buffer) {
    if (!name || !buffer || !root_vfs) return -EINVAL;

    char* tok = strtok((char*)name, "/");

    struct vfs_inode* search_in = root_vfs->root;

    // struct vfs_inode next;
    while (tok) {
        if (search_in->mount)
            search_in = search_in->mount->root;
        if (vfs_lookup(search_in, tok, &search_in) != 0) return -ENOENT;

        tok = strtok(NULL, "/");
    }
    *buffer = search_in;

    return 0;
}

size_t vfs_read(struct vfs_inode* inode, uint8_t* buffer, size_t count, size_t offset)
    { return inode ? (inode->mode == VFS_FILE ? inode->ops->read(inode, offset, buffer, count) : -EISDIR) : root_vfs->root->ops->read(root_vfs->root, offset, buffer, count); }
size_t vfs_write(struct vfs_inode* inode, uint8_t* buffer, size_t count, size_t offset)
    { return inode->mode == VFS_FILE ? inode->ops->write(inode, offset, buffer, count) : -EISDIR; }

int vfs_mount(const char* name, struct vfs_filesystem* vfs) {
    if (!vfs) return -EINVAL;

    struct vfs_inode* inode;
    if (vfs_lookup_path(name, &inode) != 0) return -ENOENT;

    if (!inode->mount) {
        inode->mount = kmalloc(sizeof(struct mount));
        if (!inode->mount) return -ENOMEM;
    }
    inode->mount->fs = vfs;
    inode->mount->mountpoint = inode;
    inode->mount->root = vfs->root;

    return 0;
}

int vfs_umount(const char* name) {
    struct vfs_inode* inode;
    if (vfs_lookup_path(name, &inode) != 0) return -ENOENT;

    if (inode->mount) kfree(inode->mount);
    inode->mount = NULL;

    return 0;
}

int vfs_mkdir(struct vfs_inode* inode, const char* name)
{
    // printf("%p %p %p %d %p %p\n", inode->ops->mkdir, inode, name, inode->ops == vfs_ops[VFS_OPS_DEVFS], inode->ops, vfs_ops[VFS_OPS_DEVFS]);
    // for(;;);

    return inode->ops->mkdir(inode, name);
}

int vfs_mkdir_path(const char* name) {
    // printf("mkdir %s\n", name);
    if (!name) return -EINVAL;

    char* tok = strtok((char*)name, "/");

    struct vfs_inode* search_in = root_vfs->root;

    while (tok) {
        if (search_in->mount) {
            search_in = search_in->mount->root;
            // printf("MOUNT\n"); for(;;);
        }
        if (vfs_lookup(search_in, tok, &search_in) != 0) {
            // this is the last directory, the next one dosen't exists yet
            return vfs_mkdir(search_in, tok);
        }

        tok = strtok(NULL, "/");
    }

    return -ENOMEM;
}