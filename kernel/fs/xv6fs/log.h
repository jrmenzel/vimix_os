/* SPDX-License-Identifier: MIT */
#pragma once

// Simple logging that allows concurrent FS system calls.
//
// A log transaction contains the updates of multiple FS system
// calls. The logging system only commits when there are
// no FS system calls active. Thus there is never
// any reasoning required about whether a commit might
// write an uncommitted system call's updates to disk.
//
// A system call should call log_begin_fs_transaction()/log_end_fs_transaction()
// to mark its start and end. Usually log_begin_fs_transaction() just increments
// the count of in-progress FS system calls and returns.
// But if it thinks the log is close to running out, it
// sleeps until the last outstanding log_end_fs_transaction() commits.
//
// The log is a physical re-do log containing disk blocks.
// The on-disk log format:
//   header block, containing block #s for block A, B, C, ...
//   block A
//   block B
//   block C
//   ...
// Log appends are synchronous.

#include <kernel/buf.h>
#include <kernel/xv6fs.h>

void log_init(int, struct xv6fs_superblock*);

/// Caller has modified b->data and is done with the buffer.
/// Record the block number and pin in the cache by increasing refcnt.
/// commit()/write_log() will do the disk write.
///
/// log_write() replaces bio_write(); a typical use is:
///   bp = bio_read(...)
///   modify bp->data[]
///   log_write(bp)
///   bio_release(bp)
void log_write(struct buf*);
void log_begin_fs_transaction();
void log_end_fs_transaction();
