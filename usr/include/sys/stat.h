/* SPDX-License-Identifier: MIT */
#pragma once

#include <stdint.h>
#include <sys/types.h>

#include <kernel/stat.h>

/// @brief Get file status
/// @param path Path to the file.
/// @param buffer Buffer for teh stat struct to be written into.
/// @return -1 on failure, 0 otherwise
int stat(const char *path, struct stat *buffer);

/// @brief make a directory, special file, or regular file
/// @param path File path of the new node
/// @param dev_major Mayor device number
/// @param dev_minor Minor device number
/// @return -1 on failure, 0 otherwise
extern int mknod(const char *path, uint16_t dev_major, uint16_t dev_minor);

/// @brief make directory
/// @param path Directory name
/// @return -1 on failure, 0 otherwise
extern int mkdir(const char *path);

/// @brief get file status
/// @param fd File descriptor of file to get stat from
/// @param buffer buffer to copy stat into
/// @return -1 on failure, 0 otherwise
extern int fstat(FILE_DESCRIPTOR fd, struct stat *buffer);
