/* SPDX-License-Identifier: MIT */
#pragma once

// Virtual File System

#include <fs/vfs_operations.h>
#include <kernel/fs.h>
#include <kernel/kernel.h>

#define FILE_SYSTEM_NAME_LENGTH 8
#define MAX_FILE_SYSTEM_TYPES 2
#define MAX_MOUNTED_FILE_SYSTEMS 4

// A file system like xv6fs
struct file_system_type
{
    // char name[FILE_SYSTEM_NAME_LENGTH]; // short identifier
    const char *name;  //< short identifier

    // shutdown
    // void (*kill_sb)(struct super_block *sb);

    /// @brief Set s_type of super_block.
    /// Opens the block device and tests if the FS is supported.
    /// @param data optional mount parameters
    /// @return 0 on success. -1 on failure (e.g. wrong FS)
    int (*fill_super_block)(struct super_block *sb, const void *data);

    struct file_system_type *next;
};

// Holds one entry in a linked list per supported file system.
extern struct file_system_type *g_file_systems;

void init_virtual_file_system();

/// Every file system needs to call this to add support to the OS
void register_file_system(struct file_system_type *fs);

/// @brief Find a file system based on its name. len = string length of name.
struct file_system_type **find_filesystem(const char *name, unsigned len);
