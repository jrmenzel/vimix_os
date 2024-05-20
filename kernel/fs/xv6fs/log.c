/* SPDX-License-Identifier: MIT */

#include <fs/xv6fs/log.h>
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

struct log
{
    struct spinlock lock;
    int32_t start;
    int32_t size;
    int32_t outstanding;  // how many FS sys calls are executing.
    int32_t committing;   // in commit(), please wait.
    dev_t dev;
    struct xv6fs_log_header lh;
};
struct log g_log;

static void recover_from_log();
static void commit();

void log_init(dev_t dev, struct xv6fs_superblock *sb)
{
    spin_lock_init(&g_log.lock, "log");
    g_log.start = sb->logstart;
    g_log.size = sb->nlog;
    g_log.dev = dev;

    // if the FS was not shutdown correctly and a log was uncommitted,
    // finish the log write now:
    recover_from_log();
}

/// @brief Copy committed blocks from g_log to their home location
/// @param recovering true if the FS is being recovered (called at each FS init,
/// but will only find an uncommited log after a system crash
static void install_trans(bool recovering)
{
    if (recovering && g_log.lh.n != 0)
    {
        int minor = MINOR(g_log.dev);
        int major = MAJOR(g_log.dev);
        printk(
            "xv6fs: Replaying %d uncommited filesystem transactions on device "
            "(%d,%d)\n",
            g_log.lh.n, minor, major);
    }
    for (int32_t tail = 0; tail < g_log.lh.n; tail++)
    {
        struct buf *lbuf =
            bio_read(g_log.dev, g_log.start + tail + 1);  // read log block
        struct buf *dbuf =
            bio_read(g_log.dev, g_log.lh.block[tail]);  // read dst
        memmove(dbuf->data, lbuf->data, BLOCK_SIZE);    // copy block to dst
        bio_write(dbuf);                                // write dst to disk
        if (recovering == false)
        {
            bio_unpin(dbuf);
        }
        bio_release(lbuf);
        bio_release(dbuf);
    }
}

/// Read the log header from disk into the in-memory log header
static void read_head()
{
    struct buf *buf = bio_read(g_log.dev, g_log.start);
    struct xv6fs_log_header *lh = (struct xv6fs_log_header *)(buf->data);
    g_log.lh.n = lh->n;

    for (size_t i = 0; i < g_log.lh.n; i++)
    {
        g_log.lh.block[i] = lh->block[i];
    }
    bio_release(buf);
}

/// Write in-memory log header to disk.
/// This is the true point at which the
/// current transaction commits.
static void write_head()
{
    struct buf *buf = bio_read(g_log.dev, g_log.start);
    struct xv6fs_log_header *hb = (struct xv6fs_log_header *)(buf->data);
    hb->n = g_log.lh.n;
    for (size_t i = 0; i < g_log.lh.n; i++)
    {
        hb->block[i] = g_log.lh.block[i];
    }
    bio_write(buf);
    bio_release(buf);
}

static void recover_from_log()
{
    read_head();
    install_trans(true);  // if committed, copy from g_log to disk
    g_log.lh.n = 0;
    write_head();  // clear the log
}

/// called at the start of each FS system call.
void log_begin_fs_transaction()
{
    spin_lock(&g_log.lock);
    while (true)
    {
        if (g_log.committing)
        {
            sleep(&g_log, &g_log.lock);
        }
        else if (g_log.lh.n + (g_log.outstanding + 1) * MAX_OP_BLOCKS > LOGSIZE)
        {
            // this op might exhaust g_log space; wait for commit.
            sleep(&g_log, &g_log.lock);
        }
        else
        {
            g_log.outstanding += 1;
            spin_unlock(&g_log.lock);
            break;
        }
    }
}

/// called at the end of each FS system call.
/// commits if this was the last outstanding operation.
void log_end_fs_transaction()
{
    bool do_commit = false;

    spin_lock(&g_log.lock);
    g_log.outstanding -= 1;
    if (g_log.committing)
    {
        panic("g_log.committing");
    }
    if (g_log.outstanding == 0)
    {
        do_commit = true;
        g_log.committing = 1;
    }
    else
    {
        // log_begin_fs_transaction() may be waiting for g_log space,
        // and decrementing g_log.outstanding has decreased
        // the amount of reserved space.
        wakeup(&g_log);
    }
    spin_unlock(&g_log.lock);

    if (do_commit)
    {
        // call commit w/o holding locks, since not allowed
        // to sleep with locks.
        commit();
        spin_lock(&g_log.lock);
        g_log.committing = 0;
        wakeup(&g_log);
        spin_unlock(&g_log.lock);
    }
}

/// Copy modified blocks from cache to log.
static void write_log()
{
    for (int32_t tail = 0; tail < g_log.lh.n; tail++)
    {
        struct buf *to =
            bio_read(g_log.dev, g_log.start + tail + 1);  // log block
        struct buf *from =
            bio_read(g_log.dev, g_log.lh.block[tail]);  // cache block
        memmove(to->data, from->data, BLOCK_SIZE);
        bio_write(to);  // write the log
        bio_release(from);
        bio_release(to);
    }
}

static void commit()
{
    if (g_log.lh.n > 0)
    {
        write_log();           // Write modified blocks from cache to log
        write_head();          // Write header to disk -- the real commit
        install_trans(false);  // Now install writes to home locations
        g_log.lh.n = 0;
        write_head();  // Erase the transaction from the log
    }
}

void log_write(struct buf *b)
{
    spin_lock(&g_log.lock);
    if (g_log.lh.n >= LOGSIZE || g_log.lh.n >= g_log.size - 1)
    {
        panic("too big a transaction");
    }
    if (g_log.outstanding < 1)
    {
        panic("log_write outside of transaction");
    }

    int32_t i;
    for (i = 0; i < g_log.lh.n; i++)
    {
        if (g_log.lh.block[i] == b->blockno)  // log absorption
        {
            break;
        }
    }
    g_log.lh.block[i] = b->blockno;
    if (i == g_log.lh.n)
    {
        // Add new block to log?
        bio_pin(b);
        g_log.lh.n++;
    }
    spin_unlock(&g_log.lock);
}
