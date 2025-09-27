/* SPDX-License-Identifier: MIT */
#pragma once

#include <fs/sysfs/sysfs_sb_priv.h>
#include <kernel/fs.h>
#include <kernel/kernel.h>
#include <kernel/kobject.h>
#include <kernel/list.h>

struct sysfs_node
{
    ino_t inode_number;  ///< inode number of this entry
    const char *name;    ///< name of this entry

    struct kobject *kobj;  ///< associated kobject
    size_t sysfs_node_index;
    struct sysfs_attribute
        *attribute;  ///< sysfs attribute, NULL for dir itself

    struct sysfs_inode *sysfs_ip;  ///< associated sysfs inode

    struct sysfs_node *parent;      ///< parent dir
    struct list_head child_list;    ///< list of child sysfs_inodes
    struct list_head sibling_list;  ///< node in parent's child_list
};

#define sysfs_node_from_child_list(ptr) \
    container_of(ptr, struct sysfs_node, sibling_list)

void sysfs_nodes_init(struct super_block *sb);
void sysfs_nodes_deinit(struct super_block *sb);

struct sysfs_node *sysfs_node_alloc_init(struct kobject *kobj,
                                         size_t sysfs_entry_index,
                                         struct sysfs_sb_private *priv);
void sysfs_node_free(struct sysfs_node *node, struct sysfs_sb_private *priv);

struct sysfs_node *sysfs_find_node(struct sysfs_sb_private *priv, ino_t inum);

void debug_print_sysfs_node(struct sysfs_node *node);
