/* SPDX-License-Identifier: MIT */
#pragma once

#include <kernel/dirent.h>

/// @brief Opens a directory stream based on a dir name.
/// @param name Name of the directory.
/// @return NULL on error.
DIR *opendir(const char *name);

/// @brief Opens a directory stream based on a file descriptor.
/// @param name Name of the directory.
/// @return NULL on error.
DIR *fdopendir(int fd);

/// @brief Iterates over all entries of a directory.
/// @return NULL on error or at the end of the directory stream.
struct dirent *readdir(DIR *dirp);

/// @brief Rewind directory stream to beginning of dir.
/// @param dirp Dir to rewind.
void rewinddir(DIR *dirp);

/// @brief get current location in directory stream, apps should make no
/// assumptions about the value.
/// @param dirp Dir
/// @return -1 on error
long telldir(DIR *dirp);

/// @brief set next dir entry to return from readdir()
/// @param dirp Dir
/// @param loc a value previously obtained from telldir()
void seekdir(DIR *dirp, long loc);

/// @brief Closes a directory stream.
/// @param dirp Stream to close.
/// @return 0 on success, -1 on error.
int closedir(DIR *dirp);
