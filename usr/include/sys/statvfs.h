/* SPDX-License-Identifier: MIT */
#pragma once

#include <kernel/statvfs.h>
#include <stdint.h>
#include <sys/types.h>

/// @brief Returns statistics about a file system.
/// @param path Any valid path inside of the file system.
/// @param buf Buffer to fill.
/// @return 0 on success, -1 on failure and errno will be set.
int statvfs(const char *path, struct statvfs *buf);

/// @brief Returns statistics about a file system.
/// @param fd Any valid file descriptor inside of the file system.
/// @param buf Buffer to fill.
/// @return 0 on success, -1 on failure and errno will be set.
int fstatvfs(int fd, struct statvfs *buf);
