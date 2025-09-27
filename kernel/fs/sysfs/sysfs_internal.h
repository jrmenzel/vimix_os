/* SPDX-License-Identifier: MIT */
#pragma once

#include <kernel/container_of.h>
#include <kernel/fs.h>
#include <kernel/kernel.h>
#include <kernel/list.h>

struct sysfs_node;

struct sysfs_inode
{
    struct inode ino;         ///< base inode
    struct sysfs_node *node;  ///< associated sysfs node
};

#define sysfs_inode_from_inode(ptr) container_of(ptr, struct sysfs_inode, ino)
