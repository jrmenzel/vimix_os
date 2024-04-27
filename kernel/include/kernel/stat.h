/* SPDX-License-Identifier: MIT */
#pragma once

#include <kernel/kernel.h>

#define XV6_FT_DIR 1     ///< Directory
#define XV6_FT_FILE 2    ///< File
#define XV6_FT_DEVICE 3  ///< Device

struct stat
{
    dev_t st_dev;      ///< File system's disk device
    uint32_t st_ino;   ///< Inode number
    short type;        ///< Type of file
    int16_t st_nlink;  ///< Number of links to file
    size_t st_size;    ///< Size of file in bytes
};
