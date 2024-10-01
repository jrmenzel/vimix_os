/* SPDX-License-Identifier: MIT */
#pragma once

// Both the kernel and user programs use this header file.
// Only include the defines that tools like mkfs need (so they don't
// have to include kernel internal headers which might clash
// with system headers from the host)

#include <kernel/kernel.h>

#define ROOT_INODE 1  // root i-number

/// Magic number to identify a xv6 filesystem
#define XV6FS_MAGIC 0x10203040

/// Number of blocks a file points to directly
#define XV6FS_N_DIRECT_BLOCKS 12

/// Number of blocks a file points to indirectly
#define XV6FS_N_INDIRECT_BLOCKS (BLOCK_SIZE / sizeof(uint32_t))

/// Max file size in blocks (== kb)
#define XV6FS_MAX_FILE_SIZE_BLOCKS \
    (XV6FS_N_DIRECT_BLOCKS + XV6FS_N_INDIRECT_BLOCKS)

// dublicate for mkfs
#ifndef BLOCK_SIZE
#define BLOCK_SIZE 1024
#endif

/// Inodes per block.
#define XV6FS_INODES_PER_BLOCK (BLOCK_SIZE / sizeof(struct xv6fs_dinode))

/// Block containing inode i
#define XV6FS_BLOCK_OF_INODE(i, sb) \
    ((i) / XV6FS_INODES_PER_BLOCK + sb.inodestart)

/// Max file name length (without the NULL-terminator)
#define XV6_NAME_MAX 14

// values of inode types:
#define XV6_FT_UNUSED 0        ///< init value
#define XV6_FT_DIR 1           ///< Directory
#define XV6_FT_FILE 2          ///< File
#define XV6_FT_CHAR_DEVICE 3   ///< Character Device
#define XV6_FT_BLOCK_DEVICE 4  ///< Block Device

/// Bitmap bits per block
#define XV6FS_BMAP_BITS_PER_BLOCK (BLOCK_SIZE * 8)

/// Block of free map containing bit for block b
#define XV6FS_BMAP_BLOCK_OF_BIT(b, bmapstart) \
    ((b) / XV6FS_BMAP_BITS_PER_BLOCK + bmapstart)

#define XV6_BLOCKS_FOR_BITMAP(size_in_blocks) \
    ((size_in_blocks) / XV6FS_BMAP_BITS_PER_BLOCK + 1)

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
_Static_assert((sizeof(struct xv6fs_superblock) < BLOCK_SIZE),
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
    uint32_t addrs[XV6FS_N_DIRECT_BLOCKS + 1];  ///< Data block addresses
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

/// Contents of the header block, used for both the on-disk header block
/// and to keep track in memory of logged block# before commit.
struct xv6fs_log_header
{
    int32_t n;
    int32_t block[LOGSIZE];
};
_Static_assert(
    (sizeof(struct xv6fs_log_header) < BLOCK_SIZE),
    "Size incorrect for xv6fs_log_header! Must be smaller than one page.");
