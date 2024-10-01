/* SPDX-License-Identifier: MIT */
#pragma once

#include <kernel/kernel.h>
#include <kernel/sleeplock.h>

/// block size of data in struct buf (in bytes)
/// Should be a multiple of the disks sector size (often 512b)
#define BLOCK_SIZE 1024

/// @brief One disk buffer / cache entry.
/// The buffers are stored and accessed via g_buf_cache in bio.c.
struct buf
{
    int32_t valid;             ///< has data been read from disk?
    int32_t disk;              ///< does disk "own" buf?
    dev_t dev;                 ///< device number of the block device
    uint32_t blockno;          ///< block number
    struct sleeplock lock;     ///< Access mutex
    uint32_t refcnt;           ///< reference count, 0 == unused
    struct buf *prev;          ///< Least recently used (LRU) cache list
    struct buf *next;          ///< Most recently used (MRU) cache list
    uint8_t data[BLOCK_SIZE];  ///< payload data from the disk
};
