/* SPDX-License-Identifier: MIT */
#pragma once

#include <stddef.h>

/// @brief Copy a file from src to dst.
/// @param src Must be a regular file.
/// @param dst Must be a filename, not a dir.
/// @return 0 on success.
int copy_file(const char *src, const char *dst);

/// @brief Move a file from one file system to another one. Uses copy_file() to
/// copy to a temp file, copy the meta data and move the copy in place. Then
/// delete the source.
/// @param src Must be a regular file.
/// @param dst Must be a filename, not a dir.
/// @return 0 on success.
int move_file_across_filesystems(const char *src, const char *dst);
