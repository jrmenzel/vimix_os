/* SPDX-License-Identifier: MIT */
#pragma once

#include <kernel/fs.h>
#include <kernel/kernel.h>

/// Return the disk block address of the nth block in inode ip.
/// If there is no such block, bmap allocates one.
/// returns 0 if out of disk space.
size_t bmap_get_block_address(struct inode *ip, uint32_t block_number);

/// @brief Allocates and inits (zeroes) a block and marks it used in the block
/// bitmap.
/// @param sb Super block to allocate from.
/// @return Block ID or 0 if out of blocks.
uint32_t block_alloc_init(struct super_block *sb);

/// @brief Frees a block, marks it free in the block bitmap.
/// @param sb Super block b belongs to.
/// @param b Block ID to free.
void block_free(struct super_block *sb, uint32_t block_id);
