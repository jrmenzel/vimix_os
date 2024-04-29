/* SPDX-License-Identifier: MIT */
#pragma once

#include <kernel/limits.h>
#include <kernel/stat.h>

/// Max file name (MAX_NAME is without `\0`). E.g. 256 on Linux
#define MAX_DIRENT_NAME (NAME_MAX + 1)

/// @brief A single directory entry (inode + file name + meta data)
/// Used by kernel and user space apps
struct dirent
{
    ino_t d_ino;  ///< Inode number
    long d_off;   ///< Named offset for historic reasons; value from/for
                  ///< telldir()/seekdir()
    unsigned short d_reclen;  ///< Length of this record in bytes
    unsigned char d_type;     ///< Type of file to prevent many fstat() calls
                              ///< (defines in stat.h)
    char d_name[MAX_DIRENT_NAME];  ///< Null-terminated file name
};

/// @brief A directory, not to be accessed by user space apps directly.
struct DIR_INTERNAL
{
    long next_entry;          ///< next entry for readdir()
    int fd;                   ///< file descriptor of the open dir
    struct dirent dir_entry;  ///< stores the most recent entry per DIR
};

typedef struct DIR_INTERNAL DIR;

/// @brief Syscall to get directory entries. Apps should use the other functions
/// in dirent.h because there is no standard on the actual syscall.
/// Here only one struct dirent is returned, which is inefficient (many syscalls
/// to list a directory). Ideally a syscall would return an array of variable
/// length dir entries (based on the actual length of the file name).
/// Linux seems to do this. But to avoid the added complexity in the syscall and
/// the stdc lib wrapper, this inefficient but simple implementation was chosen.
/// @param fd Directory file descriptor
/// @param dirp user space allocated buffer
/// @param seek_pos Position of the dir entry to query. Start at 0, then use
/// what get_dirent() returned previously.
/// @return next seek_pos on success, 0 on dir end and -1 on error.
size_t get_dirent(int fd, struct dirent *dirp, ssize_t seek_pos);
