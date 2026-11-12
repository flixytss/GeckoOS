#include "fs/vfs.h"
#include "fs/types/fat32_vfs.h"
#include "fs/types/devfs.h"
#include "fs/fs.h"
#include "mem/mmu.h"
#include "sys/std.h"
#include "sys/types.h"
#include <terminal/printf.h>
#include "sys/errno.h"
#include "terminal/terminal.h"

struct vfs_inode_ops* vfs_ops[4]; // 0 = FAT32, 1 = devfs

struct vfs_filesystem* vfs[16];
struct vfs_filesystem* root_vfs;
static struct vfs_inode* vfs_cache;

void vfs_fat32_init() {
    if (vfs_ops[VFS_OPS_FAT32]) return;

    struct vfs_inode_ops* ops = (vfs_ops[VFS_OPS_FAT32] = kmalloc(sizeof(struct vfs_inode_ops)));
    ops->read = fat32_vfs_read_file;
    ops->lookup = fat32_vfs_lookup;
    ops->write = fat32_vfs_write_file;
    ops->mkdir = fat32_vfs_mkdir;
    ops->create = fat32_vfs_create;
}
void vfs_devfs_init() {
    if (vfs_ops[VFS_OPS_DEVFS]) return;

    struct vfs_inode_ops* ops = (vfs_ops[VFS_OPS_DEVFS] = kmalloc(sizeof(struct vfs_inode_ops)));
    ops->read = devfs_vfs_read;
    ops->lookup = devfs_vfs_lookup;
    ops->write = devfs_vfs_write;
    ops->mkdir = devfs_vfs_mkdir;
    ops->create = devfs_vfs_create;
}

struct vfs_inode* vfs_inode_find(struct vfs_filesystem* fs, u64 ino) {
    struct vfs_inode* curl = vfs_cache;

    while (curl) {
        if (curl->fs == fs && curl->ino == ino) return curl;

        curl = curl->next;
    }

    return NULL;
}
struct vfs_inode* vfs_inode_create(struct vfs_filesystem* fs, u64 ino) {
    if (!fs) return NULL;

    struct vfs_inode* inode;
    if ((inode = vfs_inode_find(fs, ino)))
        return inode;

    inode = kmalloc(sizeof(struct vfs_inode));
    if (!inode) return NULL;

    memset(inode, 0, sizeof(struct vfs_inode));

    inode->fs = fs;
    inode->ino = ino;

    inode->next = vfs_cache;
    vfs_cache = inode;

    return inode;
}
int vfs_add_fs(struct drive_fs_t* fs) {
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

    return index;
}
int vfs_add_vfs(struct vfs_filesystem* fs) {
    int index = 0;
    while (vfs[index]) index++;

    vfs[index] = fs;

    return index;
}

struct vfs_filesystem* get_vfs_(int index) { return vfs[index]; }
void vfs_set_root_(struct vfs_filesystem* vfs) { root_vfs = vfs; }

int vfs_lookup(struct vfs_inode* inode, const char* name, struct vfs_inode** buffer)
    { return inode->ops->lookup(inode, name, buffer); }

int vfs_lookup_path(const char* path, struct vfs_inode** buffer) {
    char tmp[PATH_MAX];
    strncpy(tmp, path, PATH_MAX - 1);
    tmp[PATH_MAX - 1] = 0;

    char* tok = strtok(tmp, "/");

    struct vfs_inode* search_in = root_vfs->root;
    if (!search_in) return -EINVAL;

    while (tok) {
        if (vfs_lookup(search_in, tok, &search_in) != 0)
            return -ENOENT;
        if (search_in->mount)
            search_in = search_in->mount->root;

        tok = strtok(NULL, "/");
    }
    *buffer = search_in;

    return 0;
}

size_t vfs_read(struct vfs_inode* inode, u8* buffer, size_t count, size_t offset)
    { return inode->mode == VFS_FILE ? inode->ops->read(inode, offset, buffer, count) : -EISDIR; }
size_t vfs_read_path(const char* path, u8* buffer, size_t count, size_t offset) {
    struct vfs_inode* inode;
    if (0 != vfs_lookup_path(path, &inode)) return -ENOENT;
    if (VFS_DIR == inode->mode) return -EISDIR;

    return inode->ops->read(inode, offset, buffer, count);
}

size_t vfs_write(struct vfs_inode* inode, const u8* buffer, size_t count, size_t offset)
    { return inode->mode == VFS_FILE ? inode->ops->write(inode, offset, buffer, count) : -EISDIR; }
size_t vfs_write_path(const char* path, const u8* buffer, size_t count, size_t offset) {
    struct vfs_inode* inode;
    if (0 != vfs_lookup_path(path, &inode)) return -ENOENT;
    if (VFS_DIR == inode->mode) return -EISDIR;

    return inode->ops->write(inode, offset, buffer, count);
}

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

int vfs_mkdir(struct vfs_inode* inode, const char* name) {
    if (!inode) return -EINVAL;
    return inode->ops->mkdir(inode, name);
}

int vfs_mkdir_path(const char* path) {
    if (!path) return -EINVAL;

    char tmp[PATH_MAX];
    strncpy(tmp, path, PATH_MAX - 1);
    tmp[PATH_MAX - 1] = 0;
    char* tok = strtok(tmp, "/");

    struct vfs_inode* search_in = root_vfs->root;
    if (!search_in) return -EINVAL;

    while (tok) {
        if (search_in->mount)
            search_in = search_in->mount->root;
        if (vfs_lookup(search_in, tok, &search_in) != 0)
            return vfs_mkdir(search_in, tok);

        tok = strtok(NULL, "/");
    }

    return -ENOMEM;
}

int vfs_delete(struct vfs_inode* inode) {
    if (!inode) return -EINVAL;

    return inode->ops->delete(inode);
}

struct vfs_file* vfs_open_path(const char* path, u32 flags) {
    struct vfs_file* file;
    struct vfs_inode* inode;

    if (0 != vfs_lookup_path(path, &inode)) return NULL;
    if (VFS_DIR == inode->mode) return NULL;

    file = kmalloc(sizeof(struct vfs_file));
    if (!file) goto file_kmalloc_err;
    memset(file, 0, sizeof(struct vfs_file));

    file->inode = inode;
    file->flags = flags;

    return file;

file_kmalloc_err:
    pretty_printf("var file: no memory available")

    return NULL;
}

struct vfs_file* vfs_open(struct vfs_inode* inode, u32 flags) {
    struct vfs_file* file;

    if (VFS_DIR == inode->mode) return NULL;

    file = kmalloc(sizeof(struct vfs_file));
    if (!file) goto file_kmalloc_err;
    memset(file, 0, sizeof(struct vfs_file));

    file->inode = inode;
    file->flags = flags;

    return file;

file_kmalloc_err:
    pretty_printf("var file: no memory available")

    return NULL;
}

int vfs_read_file(struct vfs_file* file, u8* buffer, size_t count) {
    if (!(file->flags & P_FILE_R)) return -EBADF;

    int ret = vfs_read(file->inode, buffer, count, file->offset);
    file->offset += count;

    return ret;
}
int vfs_write_file(struct vfs_file* file, const u8* buffer, size_t count) {
    if (!(file->flags & P_FILE_W)) return -EBADF;

    int ret = vfs_write(file->inode, buffer, count, file->offset);
    file->offset += count;

    return ret;
}

void vfs_close(struct vfs_file* file)
    { kfree(file); }

int vfs_create(struct vfs_inode* inode, const char* name)
    { return inode->ops->create(inode, name); }

int vfs_create_path(const char* path) {
    char tmp[PATH_MAX];
    char* tok;
    struct vfs_inode* inode;

    strncpy(tmp, path, PATH_MAX - 1);
    tmp[PATH_MAX - 1] = 0;

    tok = strtok(tmp, "/");
    inode = root_vfs->root; // Started looking the path from the root directory

    while (tok) {
        if (inode->mount)
            inode = inode->mount->root;
        if (vfs_lookup(inode, tok, &inode) != 0)
            return vfs_create(inode, tok);

        tok = strtok(NULL, "/");
    }

    return -ENOMEM;
}

int vfs_opendir(const char* path, struct vfs_dir* dir) {
    if (!dir) return -EINVAL;

    struct vfs_inode* inode;
    if (vfs_lookup_path(path, &inode) != 0)
        return -ENOENT;
    if (inode->mode != VFS_DIR)
        return -ENOENT;

    inode->ops->opendir(inode, dir);

    return 0;
}