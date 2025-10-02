/* SPDX-License-Identifier: MIT */
#pragma once

#include <kernel/container_of.h>
#include <kernel/kernel.h>
#include <kernel/list.h>
#include <kernel/sleeplock.h>

/// block size of data in struct buf (in bytes)
/// Should be a multiple of the disks sector size (often 512b)
#define BLOCK_SIZE 1024

/// @brief One disk buffer / cache entry.
/// The buffers are stored and accessed via g_buf_cache in bio.c.
struct buf
{
    bool valid;             ///< has data been read from disk?
    int32_t disk;           ///< does disk "own" buf?
    dev_t dev;              ///< device number of the block device
    uint32_t blockno;       ///< block number
    struct sleeplock lock;  ///< Access mutex
    uint32_t refcnt;        ///< reference count, 0 == unused

    struct list_head buf_list;  ///< For linking all buffers

    size_t id;

    uint8_t data[BLOCK_SIZE];  ///< payload data from the disk
};

#define buf_from_list(ptr) container_of(ptr, struct buf, buf_list)

/// @brief Allocates and initializes a new buffer for the given device
/// and block number.
/// The buffer is added to the global buffer list.
/// The buffer is NOT locked and has a refcnt of 1.
/// @param dev The device.
/// @param blockno The block number.
/// @return The allocated buffer or NULL on error.
struct buf *buf_alloc_init(dev_t dev, uint32_t blockno);

/// @brief Initialize a buffer struct.
/// The buffer is added to the global buffer list.
/// The buffer is NOT locked and has a refcnt of 1.
/// @param b The buffer to initialize.
/// @param dev The device.
/// @param blockno The block number.
void buf_init(struct buf *b, dev_t dev, uint32_t blockno);

/// @brief Resets the buffer like after init, but does not add itself to the
/// buffer list like init.
/// @param b A buffer previously initialized with buf_init().
/// @param dev The device.
/// @param blockno The block number.
void buf_reinit(struct buf *b, dev_t dev, uint32_t blockno);

/// @brief Removes the buffer from the global buffer list.
/// The buffer must not be in use (refcnt == 0).
/// @param b The buffer to deinitialize.
void buf_deinit(struct buf *b);
