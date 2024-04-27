/* SPDX-License-Identifier: MIT */
#pragma once

#include <kernel/buf.h>
#include <kernel/kernel.h>

// Buffer cache.
//
// The buffer cache is a linked list of buf structures holding
// cached copies of disk block contents.  Caching disk blocks
// in memory reduces the number of disk reads and also provides
// a synchronization point for disk blocks used by multiple processes.
//
// Interface:
// * To get a buffer for a particular disk block, call bio_read.
// * After changing buffer data, call bio_write to write it to disk.
// * When done with the buffer, call bio_release.
// * Do not use the buffer after calling bio_release.
// * Only one process at a time can use a buffer,
//     so do not keep them longer than necessary.

/// @brief Called during boot as the first step of the filesystem init.
void bio_init();

/// @brief Return a locked buf with the contents of the indicated block.
struct buf *bio_read(dev_t dev, uint32_t blockno);

/// @brief Release a buffer. Buffer must be locked.
/// Move buffer to the head of the most-recently-used list.
/// If data was modified, call bio_write() first.
void bio_release(struct buf *b);

/// @brief Write out the changed buffer data to disk.
/// Wont release/free the buffer, call bio_release for that explicitly.
void bio_write(struct buf *b);

/// @brief Increase the buffers reference count.
void bio_pin(struct buf *b);

/// @brief Decrease the buffers reference count.
void bio_unpin(struct buf *b);
