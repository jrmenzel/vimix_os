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
