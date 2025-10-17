/* SPDX-License-Identifier: MIT */

#include <fs/vimixfs/log.h>
#include <fs/vimixfs/vimixfs.h>
#include <kernel/bio.h>
#include <kernel/buf.h>
#include <kernel/errno.h>
#include <kernel/fs.h>
#include <kernel/kernel.h>
#include <kernel/major.h>
#include <kernel/proc.h>
#include <kernel/sleeplock.h>
#include <kernel/spinlock.h>
#include <kernel/string.h>
#include <kernel/vimixfs.h>
#include <lib/minmax.h>
#include <mm/kalloc.h>

static void recover_from_log(struct log *log);
static void commit(struct log *log);

static inline ssize_t log_client_from_pid(struct log *log, pid_t pid)
{
    for (int i = 0; i < MAX_CONCURRENT_LOG_CLIENTS; i++)
    {
        if (log->clients[i] == pid)
        {
            return i;
        }
    }
    return -1;
}

ssize_t log_init(struct log *log, dev_t dev, struct vimixfs_superblock *sb)
{
    spin_lock_init(&log->lock, "log");
    log->start = sb->logstart;
    log->size = sb->nlog;
    log->dev = dev;
    log->committing = 0;
    log->blocks_used_old_clients = 0;

    log->lh_n = 0;
    log->lh_block =
        kmalloc(sizeof(uint32_t) * sb->nlog, ALLOC_FLAG_ZERO_MEMORY);
    if (log->lh_block == NULL)
    {
        printk("log_init: out of memory");
        return -ENOMEM;
    }

    memset(log->clients, 0, sizeof(log->clients));
    memset(log->blocks_used, 0, sizeof(log->blocks_used));
    memset(log->blocks_reserved, 0, sizeof(log->blocks_reserved));

    // if the FS was not shutdown correctly and a log was uncommitted,
    // finish the log write now:
    recover_from_log(log);
    return 0;
}

void log_deinit(struct log *log)
{
    kfree(log->lh_block);
    log->lh_block = NULL;
}

/// @brief Copy committed blocks from log to their home location
/// @param recovering true if the FS is being recovered (called at each FS init,
/// but will only find an uncommited log after a system crash
static void install_trans(struct log *log, bool recovering)
{
    if (recovering && log->lh_n != 0)
    {
        int minor = MINOR(log->dev);
        int major = MAJOR(log->dev);
        printk(
            "vimixfs: Replaying %d uncommited filesystem transactions on "
            "device "
            "(%d,%d)\n",
            log->lh_n, minor, major);
    }
    for (int32_t tail = 0; tail < log->lh_n; tail++)
    {
        struct buf *lbuf =
            bio_read(log->dev, log->start + tail + 1);  // read log block

        struct buf *dbuf =
            bio_get_from_cache(log->dev, log->lh_block[tail]);  // get dst
        dbuf->valid = true;

        memmove(dbuf->data, lbuf->data, BLOCK_SIZE);  // copy block to dst
        bio_write(dbuf);                              // write dst to disk
        if (recovering == false)
        {
            bio_put(dbuf);
        }
        bio_release(lbuf);
        bio_release(dbuf);
    }
}

/// Read the log header from disk into the in-memory log header
static void read_head(struct log *log)
{
    struct buf *buf = bio_read(log->dev, log->start);
    struct vimixfs_log_header *lh = (struct vimixfs_log_header *)(buf->data);
    log->lh_n = lh->n;

    for (size_t i = 0; i < log->lh_n; i++)
    {
        log->lh_block[i] = lh->block[i];
    }
    bio_release(buf);
}

/// Write in-memory log header to disk.
/// This is the true point at which the
/// current transaction commits.
static void write_head(struct log *log)
{
    // use bio_get() instead of bio_read() to avoid reading
    // log block from disk -- we don't need to read the old
    // contents, we're going to overwrite it.
    struct buf *buf = bio_get_from_cache(log->dev, log->start);
    buf->valid = true;

    struct vimixfs_log_header *hb = (struct vimixfs_log_header *)(buf->data);
    memset(hb, 0, sizeof(*hb));
    hb->n = log->lh_n;
    for (size_t i = 0; i < log->lh_n; i++)
    {
        hb->block[i] = log->lh_block[i];
    }
    bio_write(buf);
    bio_release(buf);
}

static void recover_from_log(struct log *log)
{
    read_head(log);
    install_trans(log, true);  // if committed, copy from log to disk
    log->lh_n = 0;
    write_head(log);  // clear the log
}

/// called at the start of each FS system call.
size_t log_begin_fs_transaction_explicit(struct super_block *sb,
                                         size_t request_min,
                                         size_t request_ideal)
{
    struct vimixfs_sb_private *priv =
        ((struct vimixfs_sb_private *)sb->s_fs_info);
    struct log *log = &priv->log;
    struct process *proc = get_current();
    proc->debug_log_depth++;
    if (proc->debug_log_depth != 1) panic("log begin: already called");

    size_t client_id = 0;  // guaranteed to be set
    spin_lock(&log->lock);
    while (true)
    {
        if (log->committing)
        {
            sleep(log, &log->lock);
            continue;
        }

        size_t reserved = log->blocks_used_old_clients;
        for (size_t i = 0; i < MAX_CONCURRENT_LOG_CLIENTS; i++)
        {
            reserved += log->blocks_reserved[i];
        }

        size_t available = log->size - reserved;
        if (available < request_min)
        {
            // this will exhaust log space; wait for commit.
            sleep(log, &log->lock);
            continue;
        }

        // reserve space
        size_t to_reserve = min(available, request_ideal);
        bool found_slot = false;
        for (int i = 0; i < MAX_CONCURRENT_LOG_CLIENTS; i++)
        {
            if (log->clients[i] == 0)
            {
                client_id = i;
                log->clients[i] = proc->pid;
                log->blocks_reserved[i] = to_reserve;
                log->blocks_used[i] = 0;
                found_slot = true;
                break;
            }
        }
        if (!found_slot)
        {
            sleep(log, &log->lock);
            continue;
        }

        log->outstanding += 1;
        spin_unlock(&log->lock);
        break;
    }

    return client_id;
}

/// called at the end of each FS system call.
/// commits if this was the last outstanding operation.
void log_end_fs_transaction(struct super_block *sb)
{
    struct vimixfs_sb_private *priv =
        ((struct vimixfs_sb_private *)sb->s_fs_info);
    struct log *log = &priv->log;
    bool do_commit = false;

    spin_lock(&log->lock);
    DEBUG_EXTRA_PANIC(log->committing == 0, "log should not be committing");

    struct process *proc = get_current();
    log->outstanding -= 1;
    ssize_t client = log_client_from_pid(log, proc->pid);
    DEBUG_EXTRA_PANIC(client != -1, "log_end: unknown client");
    log->clients[client] = 0;
    log->blocks_used_old_clients += log->blocks_used[client];
    log->blocks_used[client] = 0;
    log->blocks_reserved[client] = 0;

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

    proc->debug_log_depth--;
    if (proc->debug_log_depth != 0) panic("log end without log begin!");
}

/// Copy modified blocks from cache to log.
static void write_log(struct log *log)
{
    for (int32_t tail = 0; tail < log->lh_n; tail++)
    {
        // use bio_get() instead of bio_read() to avoid reading
        // log block from disk -- we don't need to read the old
        // contents, we're going to overwrite it.
        struct buf *to = bio_get_from_cache(log->dev, log->start + tail + 1);
        to->valid = true;

        struct buf *from =
            bio_read(log->dev, log->lh_block[tail]);  // cache block
        memmove(to->data, from->data, BLOCK_SIZE);
        bio_write(to);  // write the log
        bio_release(from);
        bio_release(to);
    }
}

static void commit(struct log *log)
{
    if (log->lh_n > 0)
    {
        write_log(log);             // Write modified blocks from cache to log
        write_head(log);            // Write header to disk -- the real commit
        install_trans(log, false);  // Now install writes to home locations
        log->lh_n = 0;
        write_head(log);  // Erase the transaction from the log
        log->blocks_used_old_clients = 0;
    }
}

void log_write(struct log *log, struct buf *b)
{
    spin_lock(&log->lock);
    if (log->lh_n >= log->size)
    {
        panic("too big a transaction");
    }
    if (log->outstanding < 1)
    {
        panic("log_write outside of transaction");
    }

    int32_t i;
    for (i = 0; i < log->lh_n; i++)
    {
        if (log->lh_block[i] == b->blockno)  // log absorption
        {
            break;
        }
    }

    if (i == log->lh_n)
    {
        // Add new block to log
        log->lh_block[i] = b->blockno;
        bio_get(b);
        log->lh_n++;

        struct process *proc = get_current();
        ssize_t client = log_client_from_pid(log, proc->pid);
        DEBUG_EXTRA_PANIC(client != -1, "log_write: unknown client");
        log->blocks_used[client]++;

        if (log->blocks_used[client] > log->blocks_reserved[client])
        {
            printk("log_write: client used more blocks than reserved");
            printk(" pid %d used %d, reserved %d\n", proc->pid,
                   log->blocks_used[client], log->blocks_reserved[client]);
        }
    }
    spin_unlock(&log->lock);
}

size_t log_get_client_available_blocks(struct super_block *sb, size_t client)
{
    struct vimixfs_sb_private *priv =
        ((struct vimixfs_sb_private *)sb->s_fs_info);
    struct log *log = &priv->log;

    // no locking: client is valid as it's only used by the client itself
    // and reserved blocks are static after log_begin_fs_transaction()
    return log->blocks_reserved[client] - log->blocks_used[client];
}
