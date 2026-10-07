#include "fs/vfs.h"
#include "mem.h"
#include "sys/errno.h"
#include <stddef.h>
#include <stdint.h>
#include <terminal/printf.h>

struct ramfs_node {
    struct vfs_inode* inode;

    char* name;
    struct device* device;

    struct ramfs_node* childrens; // this is a linked list
    struct ramfs_node* next;
};

struct device {
    char* name;

    size_t (*read)(struct ramfs_node* devfs, uint8_t* buffer, size_t len);
    size_t (*write)(struct ramfs_node* devfs, const uint8_t* buffer, size_t len);

    void* reserved;
};

struct vfs_filesystem* create_devfs() {
    struct vfs_filesystem* devfs = kmalloc(sizeof(struct vfs_filesystem));
    if (!devfs) return NULL;

    strcpy(devfs->name, "devfs");

    struct ramfs_node* root = kmalloc(sizeof(struct ramfs_node));
    memset(root, 0, sizeof(*root));

    root->inode = vfs_inode_create(devfs, (uint64_t)root);

    root->inode->mode = VFS_DIR;
    root->inode->fs = devfs;
    root->inode->ops = vfs_ops[VFS_OPS_DEVFS];
    root->inode->reserved = root;

    devfs->root = root->inode;

    return devfs;
}

// Vfs ops
size_t devfs_vfs_read(struct vfs_inode* inode, size_t offset, uint8_t* buffer, size_t len) {
    if (!inode || !((struct ramfs_node*)inode->reserved)->device || !buffer) return -EINVAL;
    return ((struct ramfs_node*)inode->reserved)->device->read(inode->reserved, buffer, len);
}
size_t devfs_vfs_write(struct vfs_inode* inode, size_t offset, const uint8_t* buffer, size_t len) {
    if (!inode || !((struct ramfs_node*)inode->reserved)->device || !buffer) return -EINVAL;
    return ((struct ramfs_node*)inode->reserved)->device->write(inode->reserved, buffer, len);
}
int devfs_vfs_lookup(struct vfs_inode* inode, const char* name, struct vfs_inode** buffer) {
    struct ramfs_node* node = (struct ramfs_node*)inode->reserved;
    if (!name || !buffer || !node) return -EINVAL;

    struct ramfs_node* child = node->childrens;
    while (child) {
        if (child->name) {
            // printf("looking through %s %s %p %p\n", child->name, name, child, child->next);
            if (strcmp(child->name, name) != 0) break;
            // printf("is the one %s\n", child->name);

            struct vfs_inode* inod = vfs_inode_create(inode->fs, (uint64_t)inode);
            if (!inod) return -ENOMEM;

            inod->size = child->inode->size;
            inod->mode = child->inode->mode;
            inod->ops = vfs_ops[VFS_OPS_DEVFS];
            *buffer = inod;

            return 0;
        }

        child = child->next;
    }

    return -ENOENT;
}
int devfs_vfs_mkdir(struct vfs_inode* inode, const char* name) {
    if (!inode || !name) return -EINVAL;
    struct ramfs_node* node = ((struct ramfs_node*)inode->reserved);

    struct ramfs_node* ramfs_n = kmalloc(sizeof(struct ramfs_node));
    struct vfs_inode* in = kmalloc(sizeof(struct vfs_inode));
    if (!in || !ramfs_n) return -ENOMEM;
    memset(in, 0, sizeof(struct vfs_inode));
    memset(ramfs_n, 0, sizeof(struct ramfs_node));

    ramfs_n->inode = in;
    in->fs = inode->fs;
    in->mode = VFS_DIR;
    in->ops = vfs_ops[VFS_OPS_DEVFS];

    ramfs_n->name = strdup(name);
    ramfs_n->inode->reserved = ramfs_n;

    ramfs_n->next = node->childrens;
    node->childrens = ramfs_n;

    return 0;
}



#ifdef DEBUG
    void devfs_vfs_print_node_childrens(struct vfs_inode* inode) {
        struct ramfs_node* node = ((struct ramfs_node*)inode->reserved)->childrens;
        while (node) {
            printf("Node %s at %p %p %p\n", node->name, node, node->inode, node->next);

            node = node->next;
        }
    }
#endif