/* SPDX-License-Identifier: MIT */
#pragma once

#include <kernel/buf.h>
#include <kernel/container_of.h>
#include <kernel/kernel.h>
#include <kernel/kobject.h>
#include <kernel/list.h>
#include <kernel/sleeplock.h>

/// @brief The block IO cache is a linked list of buf structures holding
/// cached copies of disk block contents. Caching disk blocks
/// in memory reduces the number of disk reads and also provides
/// a synchronization point for disk blocks used by multiple processes.
/// Interface:
/// * To get a buffer for a particular disk block, call bio_read().
/// * After changing buffer data, call bio_write() to write it to disk.
/// * When done with the buffer, call bio_release().
/// * Do not use the buffer after calling bio_release().
/// * Only one process at a time can use a buffer, so do not keep them longer
/// than necessary.
struct bio_cache
{
    struct kobject kobj;  ///< The kobject for sysfs integration.
    struct spinlock lock;

    // Linked list of all buffers.
    // Sorted by how recently the buffer was used.
    struct list_head buf_list;

    size_t num_buffers;  ///< Total number of buffers.
    size_t min_buffers;  ///< Minimum number of buffers to keep in the cache.
    size_t max_free_buffers;  ///< Try to keep at least this many free buffers.
    size_t free_buffers;      ///< Number of buffers NOT in use (refcnt==0).
};

#define bio_cache_from_kobj(ptr) container_of(ptr, struct bio_cache, kobj)

/// @brief Main buffer cache to manage BLOCK_SIZE buffers for all block devices.
extern struct bio_cache g_buf_cache;

ssize_t bio_cache_set_min_buffers(struct bio_cache *cache, ssize_t min_buffers);
ssize_t bio_cache_set_max_free_buffers(struct bio_cache *cache,
                                       ssize_t max_free_buffers);

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
void bio_get(struct buf *b);

/// @brief Decrease the buffers reference count.
void bio_put(struct buf *b);
