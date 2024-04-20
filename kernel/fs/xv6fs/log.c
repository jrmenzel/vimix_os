/* SPDX-License-Identifier: MIT */

#include <fs/xv6fs/log.h>
#include <kernel/bio.h>
#include <kernel/buf.h>
#include <kernel/fs.h>
#include <kernel/kernel.h>
#include <kernel/printk.h>
#include <kernel/proc.h>
#include <kernel/sleeplock.h>
#include <kernel/spinlock.h>
#include <kernel/string.h>
#include <kernel/types.h>

/// Contents of the header block, used for both the on-disk header block
/// and to keep track in memory of logged block# before commit.
struct logheader
{
    int n;
    int block[LOGSIZE];
};

struct log
{
    struct spinlock lock;
    int start;
    int size;
    int outstanding;  // how many FS sys calls are executing.
    int committing;   // in commit(), please wait.
    int dev;
    struct logheader lh;
};
struct log log;

static void recover_from_log(void);
static void commit();

void initlog(int dev, struct superblock *sb)
{
    if (sizeof(struct logheader) >= BSIZE) panic("initlog: too big logheader");

    initlock(&log.lock, "log");
    log.start = sb->logstart;
    log.size = sb->nlog;
    log.dev = dev;
    recover_from_log();
}

/// Copy committed blocks from log to their home location
static void install_trans(int recovering)
{
    int tail;

    for (tail = 0; tail < log.lh.n; tail++)
    {
        struct buf *lbuf =
            bread(log.dev, log.start + tail + 1);  // read log block
        struct buf *dbuf = bread(log.dev, log.lh.block[tail]);  // read dst
        memmove(dbuf->data, lbuf->data, BSIZE);  // copy block to dst
        bwrite(dbuf);                            // write dst to disk
        if (recovering == 0) bunpin(dbuf);
        brelse(lbuf);
        brelse(dbuf);
    }
}

/// Read the log header from disk into the in-memory log header
static void read_head(void)
{
    struct buf *buf = bread(log.dev, log.start);
    struct logheader *lh = (struct logheader *)(buf->data);
    int i;
    log.lh.n = lh->n;
    for (i = 0; i < log.lh.n; i++)
    {
        log.lh.block[i] = lh->block[i];
    }
    brelse(buf);
}

/// Write in-memory log header to disk.
/// This is the true point at which the
/// current transaction commits.
static void write_head(void)
{
    struct buf *buf = bread(log.dev, log.start);
    struct logheader *hb = (struct logheader *)(buf->data);
    int i;
    hb->n = log.lh.n;
    for (i = 0; i < log.lh.n; i++)
    {
        hb->block[i] = log.lh.block[i];
    }
    bwrite(buf);
    brelse(buf);
}

static void recover_from_log(void)
{
    read_head();
    install_trans(1);  // if committed, copy from log to disk
    log.lh.n = 0;
    write_head();  // clear the log
}

/// called at the start of each FS system call.
void begin_op(void)
{
    acquire(&log.lock);
    while (1)
    {
        if (log.committing)
        {
            sleep(&log, &log.lock);
        }
        else if (log.lh.n + (log.outstanding + 1) * MAXOPBLOCKS > LOGSIZE)
        {
            // this op might exhaust log space; wait for commit.
            sleep(&log, &log.lock);
        }
        else
        {
            log.outstanding += 1;
            release(&log.lock);
            break;
        }
    }
}

/// called at the end of each FS system call.
/// commits if this was the last outstanding operation.
void end_op(void)
{
    int do_commit = 0;

    acquire(&log.lock);
    log.outstanding -= 1;
    if (log.committing) panic("log.committing");
    if (log.outstanding == 0)
    {
        do_commit = 1;
        log.committing = 1;
    }
    else
    {
        // begin_op() may be waiting for log space,
        // and decrementing log.outstanding has decreased
        // the amount of reserved space.
        wakeup(&log);
    }
    release(&log.lock);

    if (do_commit)
    {
        // call commit w/o holding locks, since not allowed
        // to sleep with locks.
        commit();
        acquire(&log.lock);
        log.committing = 0;
        wakeup(&log);
        release(&log.lock);
    }
}

/// Copy modified blocks from cache to log.
static void write_log(void)
{
    int tail;

    for (tail = 0; tail < log.lh.n; tail++)
    {
        struct buf *to = bread(log.dev, log.start + tail + 1);  // log block
        struct buf *from = bread(log.dev, log.lh.block[tail]);  // cache block
        memmove(to->data, from->data, BSIZE);
        bwrite(to);  // write the log
        brelse(from);
        brelse(to);
    }
}

static void commit()
{
    if (log.lh.n > 0)
    {
        write_log();       // Write modified blocks from cache to log
        write_head();      // Write header to disk -- the real commit
        install_trans(0);  // Now install writes to home locations
        log.lh.n = 0;
        write_head();  // Erase the transaction from the log
    }
}

void log_write(struct buf *b)
{
    int i;

    acquire(&log.lock);
    if (log.lh.n >= LOGSIZE || log.lh.n >= log.size - 1)
        panic("too big a transaction");
    if (log.outstanding < 1) panic("log_write outside of trans");

    for (i = 0; i < log.lh.n; i++)
    {
        if (log.lh.block[i] == b->blockno)  // log absorption
            break;
    }
    log.lh.block[i] = b->blockno;
    if (i == log.lh.n)
    {  // Add new block to log?
        bpin(b);
        log.lh.n++;
    }
    release(&log.lock);
}
