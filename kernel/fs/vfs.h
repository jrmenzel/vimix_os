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

/// @brief Can be used for sops_alloc_inode of read-only file systems.
/// @return NULL which means no new inodes can get created.
struct inode *sops_alloc_inode_default_ro(struct super_block *sb, mode_t mode);

/// @brief Can be used for sops_write_inode of read-only
/// file systems.
int sops_write_inode_default_ro(struct inode *ip);

/// @brief Can be used for iops_create of read-only file systems.
/// @return NULL which means no new inodes can get created.
struct inode *iops_create_default_ro(struct inode *iparent, char name[NAME_MAX],
                                     mode_t mode, int32_t flags, dev_t device);

/// @brief Increases reference count of the inode.
/// @return ip to enable ip = inode_dup(ip1) idiom.
struct inode *iops_dup_default(struct inode *ip);

/// @brief Decreases ref count. Writeable filesystems should override this
/// to write back the inode if necessary.
/// @param ip The inode with held reference.
void iops_put_default(struct inode *ip);

/// @brief Default implementation of iops_dir_link for read-only file systems.
/// @return 0, as no new links can get created on a read-only file system.
int iops_dir_link_default_ro(struct inode *dir, char *name, ino_t inum);

/// @brief Default implementation of iops_unlink for read-only file systems.
/// @return -EOTHER
ssize_t iops_link_default_ro(struct inode *dir, struct inode *ip,
                             char name[NAME_MAX]);

/// @brief Default implementation of iops_unlink for read-only file systems.
/// @return 0, as no files can get deleted on a read-only file system.
ssize_t iops_unlink_default_ro(struct inode *dir, char name[NAME_MAX],
                               bool delete_files, bool delete_directories);