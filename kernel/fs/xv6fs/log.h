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

struct xv6fs_sb_private;
struct super_block;

struct log
{
    struct spinlock lock;
    int32_t start;  ///< Block number of first log block, copy from super block
    int32_t size;   ///< number of blocks, copy from super block
    int32_t outstanding;  ///< how many FS sys calls are executing.
    int32_t committing;   ///< in commit(), please wait.
    dev_t dev;      ///< device number of the block device containing the log/FS
    uint32_t lh_n;  ///< number of logged blocks
    uint32_t *lh_block;  ///< block numbers of logged blocks, dynamic array of
                         ///< size 'size'
};

/// @brief Called at FS init
/// @param log Log to initialize
/// @param dev For log->dev
/// @param sb Super block of the FS using this log
/// @return 0 on success, negative error code on failure
ssize_t log_init(struct log *log, dev_t dev, struct xv6fs_superblock *sb);

/// @brief Called at FS unmount
/// @param log Log to deinitialize
void log_deinit(struct log *log);

/// Caller has modified b->data and is done with the buffer.
/// Record the block number and pin in the cache by increasing refcnt.
/// commit()/write_log() will do the disk write.
///
/// log_write() replaces bio_write(); a typical use is:
///   bp = bio_read(...)
///   modify bp->data[]
///   log_write(bp)
///   bio_release(bp)
void log_write(struct log *log, struct buf *b);

/// @brief FS systemcalls NEED to call this before ANY FS operations.
/// Call log_end_fs_transaction() later
void log_begin_fs_transaction(struct super_block *sb);

/// @brief FS systemcalls NEED to call this after all FS operations.
void log_end_fs_transaction(struct super_block *sb);
