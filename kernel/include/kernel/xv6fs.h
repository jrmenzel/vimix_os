/* SPDX-License-Identifier: MIT */
#pragma once

// Both the kernel and user programs use this header file.
// Only include the defines that tools like mkfs need (so they don't
// have to include kernel internal headers which might clash
// with system headers from the host)

#include <kernel/kernel.h>

#define ROOT_INODE 1  // root i-number

#define XV6FS_MAGIC 0x10203040

#define NDIRECT 12
#define NINDIRECT (BLOCK_SIZE / sizeof(uint32_t))
#define MAXFILE (NDIRECT + NINDIRECT)

// dublicate for mkfs
#ifndef BLOCK_SIZE
#define BLOCK_SIZE 1024
#endif

/// Inodes per block.
#define IPB (BLOCK_SIZE / sizeof(struct xv6fs_dinode))

/// Block containing inode i
#define IBLOCK(i, sb) ((i) / IPB + sb.inodestart)

/// Max file name length (without the NULL-terminator)
#define XV6_NAME_MAX 14

// values of inode types:
#define XV6_FT_UNUSED 0        ///< init value
#define XV6_FT_DIR 1           ///< Directory
#define XV6_FT_FILE 2          ///< File
#define XV6_FT_CHAR_DEVICE 3   ///< Character Device
#define XV6_FT_BLOCK_DEVICE 4  ///< Block Device

typedef int16_t xv6fs_file_type;

/// Disk layout:
/// [ boot block | super block | log | inode blocks |
///                                          free bit map | data blocks]
///
/// mkfs computes the super block and builds an initial file system. The
/// super block describes the disk layout:
struct xv6fs_superblock
{
    uint32_t magic;       ///< Must be XV6FS_MAGIC
    uint32_t size;        ///< Size of file system image (blocks)
    uint32_t nblocks;     ///< Number of data blocks
    uint32_t ninodes;     ///< Number of inodes.
    uint32_t nlog;        ///< Number of log blocks
    uint32_t logstart;    ///< Block number of first log block
    uint32_t inodestart;  ///< Block number of first inode block
    uint32_t bmapstart;   ///< Block number of first free map block
};
_Static_assert((sizeof(struct xv6fs_superblock) < 1024),
               "xv6fs_superblock must fit in one buf->data");

/// which block on the device contains the fs superblock?
#define XV6FS_SUPER_BLOCK_NUMBER 1

/// On-disk inode structure
struct xv6fs_dinode
{
    xv6fs_file_type type;  ///< File type
    int16_t major;         ///< Major device number (XV6_FT_*_DEVICE only)
    int16_t minor;         ///< Minor device number (XV6_FT_*_DEVICE only)
    int16_t nlink;         ///< Number of links to inode in file system
    uint32_t size;         ///< Size of file (bytes)
    uint32_t addrs[NDIRECT + 1];  ///< Data block addresses
};
_Static_assert((BLOCK_SIZE % sizeof(struct xv6fs_dinode)) == 0,
               "Size of one block (1024 bytes) must be a multiple of the size "
               "of xv6fs_dinode");

#define XV6FS_UNUSED_INODE 0

/// A directory in xv6fs is a file containing a sequence of xv6fs_dirent
/// structures.
struct xv6fs_dirent
{
    uint16_t inum;
    char name[XV6_NAME_MAX];
};
_Static_assert((BLOCK_SIZE % sizeof(struct xv6fs_dirent)) == 0,
               "Size of one block (1024 bytes) must be a multiple of the size "
               "of xv6fs_dirent");
