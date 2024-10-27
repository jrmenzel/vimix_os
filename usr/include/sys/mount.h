/* SPDX-License-Identifier: MIT */
#pragma once

#include <sys/types.h>

/// @brief Mounts a file system.
/// @param source Path to the source block device.
/// @param target Path to the target directory.
/// @param filesystemtype Name of the file system type to mount.
/// @param mountflags Mount flags, not used.
/// @param data File system specific data, normally NULL.
/// @return Zero on success. -1 on error and errno is set.
extern int mount(const char *source, const char *target,
                 const char *filesystemtype, unsigned long mountflags,
                 const void *data);

/// @brief Unmounts a file system.
/// @param target Path to the target directory where the file system is mounted.
/// @return Zero on success. -1 on error and errno is set.
int umount(const char *target);
