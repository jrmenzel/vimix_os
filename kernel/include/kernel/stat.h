/* SPDX-License-Identifier: MIT */
#pragma once

#include <kernel/kernel.h>

#define XV6_FT_DIR 1     ///< Directory
#define XV6_FT_FILE 2    ///< File
#define XV6_FT_DEVICE 3  ///< Device

struct stat
{
    int st_dev;      ///< File system's disk device
    uint st_ino;     ///< Inode number
    short type;      ///< Type of file
    short st_nlink;  ///< Number of links to file
    uint64 st_size;  ///< Size of file in bytes
};
