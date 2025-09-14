/* SPDX-License-Identifier: MIT */
#pragma once

// Virtual File System

#include <fs/vfs_operations.h>
#include <kernel/fs.h>
#include <kernel/kernel.h>

// A file system like xv6fs
struct file_system_type
{
    const char *name;  //< short identifier

    // shutdown file system during umount
    void (*kill_sb)(struct super_block *sb);

    /// @brief Set s_type of super_block.
    /// Opens the block device and tests if the FS is supported.
    /// @param data optional mount parameters
    /// @return 0 on success. -ERRNO on failure (e.g. wrong FS)
    ssize_t (*init_fs_super_block)(struct super_block *sb, const void *data);

    struct file_system_type *next;
};

// Holds one entry in a linked list per supported file system.
extern struct file_system_type *g_file_systems;

void init_virtual_file_system();

/// Every file system needs to call this to add support to the OS
void register_file_system(struct file_system_type *fs);

/// @brief Find a file system based on its name. len = string length of name.
struct file_system_type **find_filesystem(const char *name, unsigned len);
