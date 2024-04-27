/* SPDX-License-Identifier: MIT */
#pragma once

#include <kernel/kernel.h>

// On-disk file system format.
// Both the kernel and user programs use this header file.

#define ROOT_INODE 1     // root i-number
#define BLOCK_SIZE 1024  // block size

/// Inodes per block.
#define IPB (BLOCK_SIZE / sizeof(struct vx6fs_dinode))

/// Block containing inode i
#define IBLOCK(i, sb) ((i) / IPB + sb.inodestart)

/// Bitmap bits per block
#define BPB (BLOCK_SIZE * 8)

/// Block of free map containing bit for block b
#define BBLOCK(b, sb) ((b) / BPB + sb.bmapstart)

/// Max file name length (without the NULL-terminator)
#define XV6_NAME_MAX 14

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

#define XV6FS_MAGIC 0x10203040

#define NDIRECT 12
#define NINDIRECT (BLOCK_SIZE / sizeof(uint32_t))
#define MAXFILE (NDIRECT + NINDIRECT)

// On-disk inode structure
struct vx6fs_dinode
{
    short type;                   ///< File type
    short major;                  ///< Major device number (XV6_FT_DEVICE only)
    short minor;                  ///< Minor device number (XV6_FT_DEVICE only)
    short nlink;                  ///< Number of links to inode in file system
    uint32_t size;                ///< Size of file (bytes)
    uint32_t addrs[NDIRECT + 1];  ///< Data block addresses
};

struct xv6fs_dirent
{
    uint16_t inum;
    char name[XV6_NAME_MAX];
};
