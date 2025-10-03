/* SPDX-License-Identifier: MIT */
#pragma once

// Both the kernel and user programs use this header file.
// Only include the defines that tools like mkfs need (so they don't
// have to include kernel internal headers which might clash
// with system headers from the host)

#include <kernel/kernel.h>
#include <kernel/time.h>

#define VIMIXFS_ROOT_INODE 1  // root i-number

/// Magic number to identify a VIMIX file system
#define VIMIXFS_MAGIC 0x10203042

/// Number of blocks a file points to directly
#define VIMIXFS_N_DIRECT_BLOCKS 21

/// Number of blocks a file points to indirectly
#define VIMIXFS_N_INDIRECT_BLOCKS (BLOCK_SIZE / sizeof(uint32_t))

/// Max file size in blocks (== kb)
#define VIMIXFS_MAX_FILE_SIZE_BLOCKS \
    (VIMIXFS_N_DIRECT_BLOCKS + VIMIXFS_N_INDIRECT_BLOCKS)

// dublicate for mkfs
#ifndef BLOCK_SIZE
#define BLOCK_SIZE 1024
#endif

/// Inodes per block.
#define VIMIXFS_INODES_PER_BLOCK (BLOCK_SIZE / sizeof(struct vimixfs_dinode))

/// Block containing inode i
#define VIMIXFS_BLOCK_OF_INODE(i, sb) \
    ((i) / VIMIXFS_INODES_PER_BLOCK + sb.inodestart)

/// Block containing inode i
#define VIMIXFS_BLOCK_OF_INODE_P(i, sb) \
    ((i) / VIMIXFS_INODES_PER_BLOCK + (sb)->inodestart)

/// Max file name length (without the NULL-terminator)
#define VIMIXFS_NAME_MAX 14

/// Mark unused inodes with mode 0
#define VIMIXFS_INVALID_MODE (0)

/// Bitmap bits per block
#define VIMIXFS_BMAP_BITS_PER_BLOCK (BLOCK_SIZE * 8)

/// Block of free map containing bit for block b
#define VIMIXFS_BMAP_BLOCK_OF_BIT(b, bmapstart) \
    ((b) / VIMIXFS_BMAP_BITS_PER_BLOCK + bmapstart)

#define VIMIXFS_BLOCKS_FOR_BITMAP(size_in_blocks) \
    ((size_in_blocks) / VIMIXFS_BMAP_BITS_PER_BLOCK + 1)

typedef int16_t vimixfs_file_type;

/// Disk layout:
/// [ boot block | super block | log | inode blocks |
///                                          free bit map | data blocks]
///
/// mkfs computes the super block and builds an initial file system. The
/// super block describes the disk layout:
struct vimixfs_superblock
{
    uint32_t magic;       ///< Must be VIMIXFS_MAGIC
    uint32_t size;        ///< Size of file system image (blocks)
    uint32_t nblocks;     ///< Number of data blocks
    uint32_t ninodes;     ///< Number of inodes.
    uint32_t nlog;        ///< Number of log blocks
    uint32_t logstart;    ///< Block number of first log block
    uint32_t inodestart;  ///< Block number of first inode block
    uint32_t bmapstart;   ///< Block number of first free map block
};
_Static_assert((sizeof(struct vimixfs_superblock) < BLOCK_SIZE),
               "vimixfs_superblock must fit in one buf->data");

/// which block on the device contains the fs superblock?
#define VIMIXFS_SUPER_BLOCK_NUMBER 1

// the following types are identical to their non-v_ counterparts on VIMIX but
// might differ on another host system. So define them explicitly to ensure mkfs
// etc work.
typedef uint32_t v_mode_t;
typedef int32_t v_dev_t;
typedef int32_t v_uid_t;
typedef int32_t v_gid_t;
typedef int64_t v_time_t;

/// On-disk inode structure
struct vimixfs_dinode
{
    v_mode_t mode;   ///< File type and permissions
    v_dev_t dev;     ///< Device number (VIMIXFS_FT_*_DEVICE only)
    uint32_t nlink;  ///< Number of links to inode in file system
    uint32_t size;   ///< Size of file (bytes)
    v_uid_t uid;     ///< User ID of owner
    v_gid_t gid;     ///< Group ID of owner
    v_time_t ctime;  ///< Creation time
    v_time_t mtime;  ///< Last modification time
    // 40 bytes so far
    uint32_t addrs[VIMIXFS_N_DIRECT_BLOCKS + 1];  ///< Data block addresses
};
_Static_assert((BLOCK_SIZE % sizeof(struct vimixfs_dinode)) == 0,
               "Size of one block (1024 bytes) must be a multiple of the size "
               "of vimixfs_dinode");

/// A directory in vimixfs is a file containing a sequence of vimixfs_dirent
/// structures.
struct vimixfs_dirent
{
    uint16_t inum;
    char name[VIMIXFS_NAME_MAX];
};
_Static_assert((BLOCK_SIZE % sizeof(struct vimixfs_dirent)) == 0,
               "Size of one block (1024 bytes) must be a multiple of the size "
               "of vimixfs_dirent");

#define VIMIXFS_MAX_LOG_BLOCKS (BLOCK_SIZE / sizeof(int32_t) - 1)

/// Contents of the header block, used for the on-disk header block.
/// Assume the maximal number of log blocks, only the required will be allocated
/// at runtime and only those will get copied. A full block is read from disk
/// anyways.
struct vimixfs_log_header
{
    int32_t n;
    int32_t block[VIMIXFS_MAX_LOG_BLOCKS];
};
_Static_assert((sizeof(struct vimixfs_log_header) <= BLOCK_SIZE),
               "Size incorrect for vimixfs_log_header! Must fit in one page.");
