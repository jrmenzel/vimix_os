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

void bio_init();
struct buf* bio_read(uint, uint);
void bio_release(struct buf*);
void bio_write(struct buf*);
void bio_pin(struct buf*);
void bio_unpin(struct buf*);
