#include "fs/vfs.h"
#include "fs/fat32.h"
#include "fs/devfs.h"
#include "mem.h"
#include "sys/types.h"
#include <terminal/printf.h>
#include "sys/errno.h"
#include "terminal/terminal.h"

struct vfs_inode_ops* vfs_ops[4]; // 0 = FAT32, 1 = devfs

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

struct vfs_filesystem* vfs[16];
struct vfs_filesystem* root_vfs;
static struct vfs_inode* vfs_cache;

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
void vfs_add_fs(struct drive_fs_t* fs) {
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
size_t vfs_write(struct vfs_inode* inode, u8* buffer, size_t count, size_t offset)
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

#define FILE_R 0x1
#define FILE_W 0x2

struct vfs_file* vfs_open(const char* path, u32 flags) {
    struct vfs_file* file;
    struct vfs_inode* inode;


    // Looking the last working sub-paths of the full path
    char tmp[PATH_MAX];
    char* tok;
    char* name;
    struct vfs_inode* in;
    struct vfs_inode* new;
    struct vfs_inode* father;

    strncpy(tmp, path, PATH_MAX - 1);
    tmp[PATH_MAX - 1] = 0;

    tok = strtok(tmp, "/");
    in = root_vfs->root; // started looking the path from the root directory

    while (tok) {
        if (vfs_lookup(in, tok, &new) != 0) {
            name = strdup(tok);
            father = in;
            break;
        }

        in = new;
        tok = strtok(NULL, "/");
    }

    file = kmalloc(sizeof(struct vfs_file));
    inode = kmalloc(sizeof(struct vfs_inode));

    if (!file) goto file_kmalloc_err;
    if (!inode) goto inode_kmalloc_err;

    memset(file, 0, sizeof(struct vfs_file));
    memset(inode, 0, sizeof(struct vfs_inode));

    file->inode = inode;
    file->flags = flags;
    file->name = name;
    
    inode->fs = father->fs;
    inode->mode = VFS_FILE;

    return file;

file_kmalloc_err:
    pretty_printf("var file: no memory available")

inode_kmalloc_err:
    pretty_printf("var inode: no memory available")

    return NULL;
}

