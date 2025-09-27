/* SPDX-License-Identifier: MIT */

#pragma once

#include <kernel/kernel.h>
#include <kernel/rwspinlock.h>
#include <kernel/stdatomic.h>

struct sysfs_node;

/// @brief Private data for sysfs super block
struct sysfs_sb_private
{
    atomic_int32_t next_free_inum;
    struct rwspinlock lock;   ///< protects the node tree
    struct sysfs_node *root;  ///< root node of the sysfs tree
};
