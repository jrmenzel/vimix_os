/* SPDX-License-Identifier: MIT */
#pragma once

#include <kernel/kernel.h>

struct statvfs
{
    size_t f_bsize;   ///< File system block size
    size_t f_frsize;  ///< Fragment size: smallest allocatable unit, usally same
                      ///< as f_bsize
    size_t f_blocks;  ///< Size of file system in f_frsize units
    size_t f_bfree;   ///< Number of free blocks
    size_t f_bavail;  ///< Number of free blocks for unprivileged users
    size_t f_files;   ///< Number of used inodes
    size_t f_ffree;   ///< Number of free inodes
    size_t f_favail;  ///< Number of free inodes for unprivileged users
    size_t f_fsid;    ///< Filesystem ID
    size_t f_flag;    ///< Mount flags
    size_t f_namemax;  ///< Maximum filename length
};
