/* SPDX-License-Identifier: MIT */
#pragma once

char *find_program_in_path(const char *program);

/// @brief Combines path and file into dst.
/// Ensures there is exactly one '/' between path and file.
/// @param dst Destination buffer of size PATH_MAX
/// @param path Path with or without trailing /
/// @param file file name
/// @return -1 on failure, 0 on success
int build_full_path(char *dst, const char *path, const char *file);

/// @brief Used when copying of moving files: src is a file name/path and dst
/// can also be a file or a directory. If it's a dir, the new_dst will be
/// dir/src_filename. Otherwise it's a copy of dst.
/// @param src File name.
/// @param dst File or directory.
/// @param new_dst Target file/path.
/// @param new_dst_size Size of new_dst buffer.
/// @return 0 on success.
int build_copy_target(const char *src, const char *dst, char *new_dst,
                      size_t new_dst_size);
