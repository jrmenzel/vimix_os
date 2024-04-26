/* SPDX-License-Identifier: MIT */
#pragma once

#include <kernel/fs.h>
#include <kernel/kernel.h>
#include <kernel/sleeplock.h>

struct buf
{
    int valid;  ///< has data been read from disk?
    int disk;   ///< does disk "own" buf?
    uint32_t dev;
    uint32_t blockno;
    struct sleeplock lock;
    uint32_t refcnt;
    struct buf *prev;  ///< LRU cache list
    struct buf *next;
    uchar data[BLOCK_SIZE];
};
