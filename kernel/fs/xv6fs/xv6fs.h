/* SPDX-License-Identifier: MIT */
#pragma once

#include <kernel/fs.h>
#include <kernel/kernel.h>
#include <kernel/xv6fs.h>

extern struct xv6fs_superblock sb;

/// Bitmap bits per block
#define BPB (BLOCK_SIZE * 8)

/// Block of free map containing bit for block b
#define BBLOCK(b, sb) ((b) / BPB + sb.bmapstart)

/// Allocate a zeroed disk block.
/// returns 0 if out of disk space.
uint32_t balloc(dev_t dev);

/// Free a disk block.
void bfree(dev_t dev, uint32_t b);

/// Return the disk block address of the nth block in inode ip.
/// If there is no such block, bmap allocates one.
/// returns 0 if out of disk space.
size_t bmap(struct inode *ip, uint32_t bn);

xv6fs_file_type imode_to_xv6_file_type(mode_t imode);

mode_t xv6_file_type_to_imode(xv6fs_file_type type);

struct inode *xv6fs_iops_alloc(dev_t dev, mode_t mode);

int xv6fs_iops_update(struct inode *ip);

int xv6fs_iops_read_in(struct inode *ip);

int xv6fs_iops_trunc(struct inode *ip);
