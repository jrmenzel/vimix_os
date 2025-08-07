/* SPDX-License-Identifier: MIT */

#include <fs/xv6fs/log.h>
#include <fs/xv6fs/xv6fs.h>
#include <kernel/bio.h>
#include <kernel/buf.h>
#include <kernel/fs.h>
#include <kernel/kernel.h>
#include <kernel/major.h>
#include <kernel/proc.h>
#include <kernel/sleeplock.h>
#include <kernel/spinlock.h>
#include <kernel/string.h>
#include <kernel/xv6fs.h>

static void recover_from_log(struct log *log);
static void commit(struct log *log);

void log_init(struct log *log, dev_t dev, struct xv6fs_superblock *sb)
{
    spin_lock_init(&log->lock, "log");
    log->start = sb->logstart;
    log->size = sb->nlog;
    log->dev = dev;
    log->committing = 0;

    // if the FS was not shutdown correctly and a log was uncommitted,
    // finish the log write now:
    recover_from_log(log);
}

/// @brief Copy committed blocks from log to their home location
/// @param recovering true if the FS is being recovered (called at each FS init,
/// but will only find an uncommited log after a system crash
static void install_trans(struct log *log, bool recovering)
{
    if (recovering && log->lh.n != 0)
    {
        int minor = MINOR(log->dev);
        int major = MAJOR(log->dev);
        printk(
            "xv6fs: Replaying %d uncommited filesystem transactions on device "
            "(%d,%d)\n",
            log->lh.n, minor, major);
    }
    for (int32_t tail = 0; tail < log->lh.n; tail++)
    {
        struct buf *lbuf =
            bio_read(log->dev, log->start + tail + 1);  // read log block
        struct buf *dbuf = bio_read(log->dev, log->lh.block[tail]);  // read dst
        memmove(dbuf->data, lbuf->data, BLOCK_SIZE);  // copy block to dst
        bio_write(dbuf);                              // write dst to disk
        if (recovering == false)
        {
            bio_unpin(dbuf);
        }
        bio_release(lbuf);
        bio_release(dbuf);
    }
}

/// Read the log header from disk into the in-memory log header
static void read_head(struct log *log)
{
    struct buf *buf = bio_read(log->dev, log->start);
    struct xv6fs_log_header *lh = (struct xv6fs_log_header *)(buf->data);
    log->lh.n = lh->n;

    for (size_t i = 0; i < log->lh.n; i++)
    {
        log->lh.block[i] = lh->block[i];
    }
    bio_release(buf);
}

/// Write in-memory log header to disk.
/// This is the true point at which the
/// current transaction commits.
static void write_head(struct log *log)
{
    struct buf *buf = bio_read(log->dev, log->start);
    struct xv6fs_log_header *hb = (struct xv6fs_log_header *)(buf->data);
    hb->n = log->lh.n;
    for (size_t i = 0; i < log->lh.n; i++)
    {
        hb->block[i] = log->lh.block[i];
    }
    bio_write(buf);
    bio_release(buf);
}

static void recover_from_log(struct log *log)
{
    read_head(log);
    install_trans(log, true);  // if committed, copy from log to disk
    log->lh.n = 0;
    write_head(log);  // clear the log
}

/// called at the start of each FS system call.
void log_begin_fs_transaction(struct super_block *sb)
{
    struct xv6fs_sb_private *priv = ((struct xv6fs_sb_private *)sb->s_fs_info);
    struct log *log = &priv->log;
    struct process *proc = get_current();
    proc->debug_log_depth++;
    if (proc->debug_log_depth != 1) panic("log begin: already called");

    spin_lock(&log->lock);
    while (true)
    {
        // worst case: assume all outstanding use max log size
        size_t worst_case_log_size_incl_this =
            log->lh.n + (log->outstanding + 1) * MAX_OP_BLOCKS;
        if (log->committing)
        {
            sleep(log, &log->lock);
        }
        else if (worst_case_log_size_incl_this > LOGSIZE)
        {
            // this op might exhaust log space; wait for commit.
            sleep(log, &log->lock);
        }
        else
        {
            log->outstanding += 1;
            spin_unlock(&log->lock);
            break;
        }
    }
}

/// called at the end of each FS system call.
/// commits if this was the last outstanding operation.
void log_end_fs_transaction(struct super_block *sb)
{
    struct xv6fs_sb_private *priv = ((struct xv6fs_sb_private *)sb->s_fs_info);
    struct log *log = &priv->log;
    bool do_commit = false;

    spin_lock(&log->lock);
    log->outstanding -= 1;
    if (log->committing)
    {
        panic("log->committing");
    }
    if (log->outstanding == 0)
    {
        do_commit = true;
        log->committing = 1;
    }
    else
    {
        // log_begin_fs_transaction() may be waiting for log space,
        // and decrementing log->outstanding has decreased
        // the amount of reserved space.
        wakeup(log);
    }
    spin_unlock(&log->lock);

    if (do_commit)
    {
        // call commit w/o holding locks, since not allowed
        // to sleep with locks.
        commit(log);
        spin_lock(&log->lock);
        log->committing = 0;
        wakeup(log);
        spin_unlock(&log->lock);
    }

    struct process *proc = get_current();
    proc->debug_log_depth--;
    if (proc->debug_log_depth != 0) panic("log end without log begin!");
}

/// Copy modified blocks from cache to log.
static void write_log(struct log *log)
{
    for (int32_t tail = 0; tail < log->lh.n; tail++)
    {
        struct buf *to =
            bio_read(log->dev, log->start + tail + 1);  // log block
        struct buf *from =
            bio_read(log->dev, log->lh.block[tail]);  // cache block
        memmove(to->data, from->data, BLOCK_SIZE);
        bio_write(to);  // write the log
        bio_release(from);
        bio_release(to);
    }
}

static void commit(struct log *log)
{
    if (log->lh.n > 0)
    {
        write_log(log);             // Write modified blocks from cache to log
        write_head(log);            // Write header to disk -- the real commit
        install_trans(log, false);  // Now install writes to home locations
        log->lh.n = 0;
        write_head(log);  // Erase the transaction from the log
    }
}

void log_write(struct log *log, struct buf *b)
{
    spin_lock(&log->lock);
    if (log->lh.n >= LOGSIZE || log->lh.n >= log->size - 1)
    {
        panic("too big a transaction");
    }
    if (log->outstanding < 1)
    {
        panic("log_write outside of transaction");
    }

    int32_t i;
    for (i = 0; i < log->lh.n; i++)
    {
        if (log->lh.block[i] == b->blockno)  // log absorption
        {
            break;
        }
    }
    log->lh.block[i] = b->blockno;
    if (i == log->lh.n)
    {
        // Add new block to log?
        bio_pin(b);
        log->lh.n++;
    }
    spin_unlock(&log->lock);
}
