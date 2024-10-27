/* SPDX-License-Identifier: MIT */
#pragma once

#include <fs/vfs.h>
#include <kernel/kernel.h>

/// @brief Mounts a file system.
/// @param source Path to the source block device.
/// @param target Path to the target directory.
/// @param filesystemtype Name of the file system type to mount.
/// @param mountflags Mount flags, not used.
/// @param data File system specific data, normally NULL.
/// @return Zero on success, -ERRNO otherwise.
ssize_t mount(const char *source, const char *target,
              const char *filesystemtype, unsigned long mountflags,
              size_t addr_data);

ssize_t mount_types(dev_t source, struct inode *i_target,
                    struct file_system_type *filesystemtype,
                    unsigned long mountflags, size_t addr_data);

/// @brief Unmounts a file system.
/// @param target Path to the target directory where the file system is mounted.
/// @return Zero on success, -ERRNO otherwise.
ssize_t umount(const char *target);

/// @brief Helper for unmount()
/// @param i_target LOCKED(!) directory inode with mounted file system
/// @param sb Super block of the file system to unmount.
/// @return Zero on success, -ERRNO otherwise.
ssize_t umount_types(struct inode *i_target_mountpoint, struct super_block *sb);
