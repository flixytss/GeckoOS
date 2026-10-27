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

    struct ramfs_node* root = kmalloc(sizeof(struct ramfs_node));
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
    return ((struct ramfs_node*)inode->reserved)->device->read(inode->reserved, buffer, len);
}
size_t devfs_vfs_write(struct vfs_inode* inode, size_t offset, const u8* buffer, size_t len) {
    return ((struct ramfs_node*)inode->reserved)->device->write(inode->reserved, buffer, len);
}
int devfs_vfs_lookup(struct vfs_inode* inode, const char* name, struct vfs_inode** buffer) {
    struct ramfs_node* node = (struct ramfs_node*)inode->reserved;

    struct ramfs_node* child = node->childrens;
    while (child) {
        if (child->name) {
            // printf("looking through %s %s %p %p\n", child->name, name, child, child->next);
            if (strcmp(child->name, name) != 0) break;
            // printf("is the one %s\n", child->name);

            struct vfs_inode* inod = vfs_inode_create(inode->fs, (u64)inode);
            if (!inod) return -ENOMEM;

            inod->size = child->inode->size;
            inod->mode = child->inode->mode;
            inod->ops = vfs_ops[VFS_OPS_DEVFS];
            *buffer = inod;
            printf("returning %s\n", child->name);

            return 0;
        }

        child = child->next;
    }

    return -ENOENT;
}
int devfs_vfs_mkdir(struct vfs_inode* inode, const char* name) {
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
    in->reserved = ramfs_n;

    ramfs_n->name = strdup(name);
    ramfs_n->next = node->childrens;

    // ramfs_n->last = node->childrens;
    node->childrens = ramfs_n;

    return 0;
}

int devfs_vfs_delete(struct vfs_inode* inode) {
    if (inode->mode == VFS_DIR) {
        struct ramfs_node* child = ((struct ramfs_node*)inode->reserved)->childrens;
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
        struct ramfs_node* node = ((struct ramfs_node*)inode->reserved)->childrens;
        printf("%p\n", ((struct ramfs_node*)get_vfs_(1)->root->reserved)->childrens);
        while (node) {
            printf("Node %s at %p %p %p %p\n", node->name, node, node->inode, node->next, node->last);

            node = node->next;
        }
    }

    void devfs_vfs_print_recursive_nodes(struct vfs_inode* inode) {
        struct ramfs_node* curl = ((struct ramfs_node*)inode->reserved)->childrens;
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
