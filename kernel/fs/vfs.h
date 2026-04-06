/* SPDX-License-Identifier: MIT */
#pragma once

// Virtual File System

#include <fs/vfs_operations.h>
#include <kernel/fs.h>
#include <kernel/kernel.h>

// A file system like vimixfs, SysFS, etc
struct file_system_type
{
    const char *name;  //< short identifier

    // shutdown file system during umount
    void (*kill_sb)(struct super_block *sb);

    /// @brief Set s_type of super_block.
    /// Opens the block device and tests if the FS is supported.
    /// @param data optional mount parameters
    /// @return 0 on success. -ERRNO on failure (e.g. wrong FS)
    syserr_t (*init_fs_super_block)(struct super_block *sb, const void *data);

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

syserr_t sops_statvfs_default(struct super_block *sb, struct statvfs *to_fill);

/// @brief Can be used for iops_create of read-only file systems.
/// @return NULL which means no new inodes can get created.
syserr_t iops_create_default_ro(struct inode *parent, struct dentry *dp,
                                mode_t mode, int32_t flags);

syserr_t iops_mknod_default_ro(struct inode *parent, struct dentry *dp,
                               mode_t mode, dev_t dev);

syserr_t iops_mkdir_default_ro(struct inode *parent, struct dentry *dp,
                               mode_t mode);

/// @brief Decreases ref count. Writeable filesystems should override this
/// to write back the inode if necessary.
/// @param ip The inode with held reference.
void iops_put_default(struct inode *ip);

/// @brief Default implementation of iops_link for read-only file systems.
/// @return -EACCES
syserr_t iops_link_default_ro(struct dentry *file_from, struct inode *dir_to,
                              struct dentry *new_link);

/// @brief Default implementation of iops_rmdir for read-only file systems.
/// @return -EACCES
syserr_t iops_rmdir_default_ro(struct inode *parent, struct dentry *dp);

/// @brief Default implementation of iops_unlink for read-only file systems.
/// @return -EACCES, as no files can get deleted on a read-only file system.
syserr_t iops_unlink_default_ro(struct inode *parent, struct dentry *dp);

/// @brief Default implementation of iops_truncate for read-only file systems.
/// @param dp Ignored.
/// @param length Ignored.
/// @return -EACCES to indicate read-only file system.
syserr_t iops_truncate_default_ro(struct dentry *dp, off_t length);

/// @brief Default implementation of iops_chmod for read-only file systems.
/// @param dp Ignored.
/// @param mode Ignored.
/// @return -EACCES to indicate read-only file system.
syserr_t iops_chmod_default_ro(struct dentry *dp, mode_t mode);

/// @brief Default implementation of iops_chown for read-only file systems.
/// @param dp Ignored.
/// @param uid Ignored.
/// @param gid Ignored.
/// @return -EACCES to indicate read-only file system.
syserr_t iops_chown_default_ro(struct dentry *dp, uid_t uid, gid_t gid);

syserr_t fops_open_default(struct inode *ip, struct file *f);
