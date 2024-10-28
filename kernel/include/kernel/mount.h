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

/// @brief Mounts the root file system. Differs from regular mount as this has
/// no inode to be mounted on. Also the source is a raw device number, no device
/// inode.
/// @param dev Device number of root file system.
/// @param filesystemtype String with the file system type (e.g. "xv6fs").
void mount_root(dev_t dev, const char *filesystemtype);

/// @brief Helper for mount() and mount_root().
///        g_mount_lock must be hold when calling this!
/// @param source Device number of block device to mount.
/// @param i_target Target directory inode or NULL to mount the root FS.
/// @param filesystemtype File system type.
/// @param mountflags ignored
/// @param addr_data ignored
/// @return Zero on success, -ERRNO otherwise.
ssize_t mount_types(dev_t source, struct inode *i_target,
                    struct file_system_type *filesystemtype,
                    unsigned long mountflags, size_t addr_data);

/// @brief Unmounts a file system.
/// @param target Path to the target directory where the file system is mounted.
/// @return Zero on success, -ERRNO otherwise.
ssize_t umount(const char *target);

/// @brief Helper for unmount()
///        g_mount_lock must be hold when calling this!
/// @param i_target LOCKED(!) directory inode with mounted file system
/// @param sb Super block of the file system to unmount.
/// @return Zero on success, -ERRNO otherwise.
ssize_t umount_types(struct inode *i_target_mountpoint, struct super_block *sb);
