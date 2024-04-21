/* SPDX-License-Identifier: MIT */
#pragma once

#include <kernel/kernel.h>

// On-disk file system format.
// Both the kernel and user programs use this header file.

#define ROOT_INODE 1     // root i-number
#define BLOCK_SIZE 1024  // block size

/// Disk layout:
/// [ boot block | super block | log | inode blocks |
///                                          free bit map | data blocks]
///
/// mkfs computes the super block and builds an initial file system. The
/// super block describes the disk layout:
struct xv6fs_superblock
{
    uint magic;       ///< Must be XV6FS_MAGIC
    uint size;        ///< Size of file system image (blocks)
    uint nblocks;     ///< Number of data blocks
    uint ninodes;     ///< Number of inodes.
    uint nlog;        ///< Number of log blocks
    uint logstart;    ///< Block number of first log block
    uint inodestart;  ///< Block number of first inode block
    uint bmapstart;   ///< Block number of first free map block
};

#define XV6FS_MAGIC 0x10203040

#define NDIRECT 12
#define NINDIRECT (BLOCK_SIZE / sizeof(uint))
#define MAXFILE (NDIRECT + NINDIRECT)

// On-disk inode structure
struct vx6fs_dinode
{
    short type;               ///< File type
    short major;              ///< Major device number (XV6_FT_DEVICE only)
    short minor;              ///< Minor device number (XV6_FT_DEVICE only)
    short nlink;              ///< Number of links to inode in file system
    uint size;                ///< Size of file (bytes)
    uint addrs[NDIRECT + 1];  ///< Data block addresses
};

/// Inodes per block.
#define IPB (BLOCK_SIZE / sizeof(struct vx6fs_dinode))

/// Block containing inode i
#define IBLOCK(i, sb) ((i) / IPB + sb.inodestart)

/// Bitmap bits per block
#define BPB (BLOCK_SIZE * 8)

/// Block of free map containing bit for block b
#define BBLOCK(b, sb) ((b) / BPB + sb.bmapstart)

/// Directory is a file containing a sequence of xv6fs_dirent structures.
#define XV6_NAME_MAX 14

struct xv6fs_dirent
{
    ushort inum;
    char name[XV6_NAME_MAX];
};
