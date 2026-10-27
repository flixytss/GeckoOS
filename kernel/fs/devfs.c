#include "fs/vfs.h"
#include "mem.h"
#include "sys/errno.h"
#include <stddef.h>
#include "sys/types.h"
#include "fs/devfs.h"
#include <terminal/printf.h>

struct vfs_filesystem* create_devfs() {
    struct vfs_filesystem* devfs = kmalloc(sizeof(struct vfs_filesystem));
    if (!devfs) return NULL;

    strcpy(devfs->name, "devfs");

    struct devfs_node* root = kmalloc(sizeof(struct devfs_node));
    memset(root, 0, sizeof(*root));

    root->inode = vfs_inode_create(devfs, (u64)root);

    root->inode->mode = VFS_DIR;
    root->inode->fs = devfs;
    root->inode->ops = vfs_ops[VFS_OPS_DEVFS];
    root->inode->reserved = root;

    root->name = strdup("devfs-root");

    devfs->root = root->inode;

    return devfs;
}

// Vfs ops
size_t devfs_vfs_read(struct vfs_inode* inode, size_t offset, u8* buffer, size_t len) {
    return ((struct devfs_node*)inode->reserved)->device->read(inode->reserved, buffer, len);
}
size_t devfs_vfs_write(struct vfs_inode* inode, size_t offset, const u8* buffer, size_t len) {
    return ((struct devfs_node*)inode->reserved)->device->write(inode->reserved, buffer, len);
}
int devfs_vfs_lookup(struct vfs_inode* inode, const char* name, struct vfs_inode** buffer) {
    struct devfs_node* node = (struct devfs_node*)inode->reserved;
    if (!node) return -ENOENT;

    struct devfs_node* child = node->childrens;

    while (child) {
        if (child->name) {
            if (strcmp(child->name, name) != 0) goto mismatch; // Human is dead, Mismatch

            struct vfs_inode* inod = vfs_inode_create(inode->fs, (u64)inode);
            if (!inod) return -ENOMEM;

            inod->size = child->inode->size;
            inod->mode = child->inode->mode;
            inod->ops = vfs_ops[VFS_OPS_DEVFS];
            inod->reserved = child;
            *buffer = inod;

            return 0;
        }

mismatch:
        child = child->next;
    }

    return -ENOENT;
}
int devfs_vfs_mkdir(struct vfs_inode* inode, const char* name) {
    struct devfs_node* node = ((struct devfs_node*)inode->reserved);

    struct devfs_node* ramfs_n = kmalloc(sizeof(struct devfs_node));
    struct vfs_inode* in = kmalloc(sizeof(struct vfs_inode));
    if (!in || !ramfs_n) return -ENOMEM;
    memset(in, 0, sizeof(struct vfs_inode));
    memset(ramfs_n, 0, sizeof(struct devfs_node));

    ramfs_n->inode = in;
    in->fs = inode->fs;
    in->mode = VFS_DIR;
    in->ops = vfs_ops[VFS_OPS_DEVFS];
    in->reserved = ramfs_n;

    ramfs_n->name = strdup(name);
    ramfs_n->next = node->childrens;
    // ramfs_n->childrens = NULL;

    node->childrens = ramfs_n;

    return 0;
}

int devfs_vfs_delete(struct vfs_inode* inode) {
    if (inode->mode == VFS_DIR) {
        struct devfs_node* child = ((struct devfs_node*)inode->reserved)->childrens;
        while (child) {
            devfs_vfs_delete(child->inode);
            child = child->next;
        }
    }

    kfree(inode);

    return 0;
}

#ifdef DEBUG
    void devfs_vfs_print_node_childrens(struct vfs_inode* inode) {
        struct devfs_node* node = ((struct devfs_node*)inode->reserved)->childrens;
        // printf("%p\n", ((struct ramfs_node*)get_vfs_(1)->root->reserved)->childrens);
        while (node) {
            if (!node->name || !node->inode) break;
            printf("Node %s at %p %p %p, is dir = %d\n", node->name, node, node->inode, node->next, node->inode->mode == VFS_DIR);

            node = node->next;
        }
    }

    void devfs_vfs_print_recursive_nodes(struct vfs_inode* inode) {
        struct devfs_node* curl = ((struct devfs_node*)inode->reserved)->childrens;
        while (curl) {
            printf("Node %s", curl->name);
            if (curl->inode->mode == VFS_DIR) {
                printf("  \n");
                devfs_vfs_print_recursive_nodes(curl->inode);
            }

            curl = curl->next;
        }
    }
#endif
