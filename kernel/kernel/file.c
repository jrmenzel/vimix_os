/* SPDX-License-Identifier: MIT */

//
// Support functions for system calls that involve file descriptors.
//

#include <fs/xv6fs/log.h>
#include <ipc/pipe.h>
#include <kernel/file.h>
#include <kernel/fs.h>
#include <kernel/kernel.h>
#include <kernel/printk.h>
#include <kernel/proc.h>
#include <kernel/sleeplock.h>
#include <kernel/spinlock.h>
#include <kernel/stat.h>
#include <kernel/vm.h>

struct devsw devsw[NDEV];
struct
{
    struct spinlock lock;
    struct file file[NFILE];
} ftable;

void file_init() { spin_lock_init(&ftable.lock, "ftable"); }

struct file *file_alloc()
{
    struct file *f;

    spin_lock(&ftable.lock);
    for (f = ftable.file; f < ftable.file + NFILE; f++)
    {
        if (f->ref == 0)
        {
            f->ref = 1;
            spin_unlock(&ftable.lock);
            return f;
        }
    }
    spin_unlock(&ftable.lock);
    return 0;
}

struct file *file_dup(struct file *f)
{
    spin_lock(&ftable.lock);
    if (f->ref < 1) panic("file_dup");
    f->ref++;
    spin_unlock(&ftable.lock);
    return f;
}

void file_close(struct file *f)
{
    struct file ff;

    spin_lock(&ftable.lock);
    if (f->ref < 1) panic("file_close");
    if (--f->ref > 0)
    {
        spin_unlock(&ftable.lock);
        return;
    }
    ff = *f;
    f->ref = 0;
    f->type = FD_NONE;
    spin_unlock(&ftable.lock);

    if (ff.type == FD_PIPE)
    {
        pipe_close(ff.pipe, ff.writable);
    }
    else if (ff.type == FD_INODE || ff.type == FD_DEVICE)
    {
        log_begin_fs_transaction();
        inode_put(ff.ip);
        log_end_fs_transaction();
    }
}

int32_t file_stat(struct file *f, size_t addr)
{
    struct process *proc = get_current();
    struct stat st;

    if (f->type == FD_INODE || f->type == FD_DEVICE)
    {
        inode_lock(f->ip);
        inode_stat(f->ip, &st);
        inode_unlock(f->ip);
        if (uvm_copy_out(proc->pagetable, addr, (char *)&st, sizeof(st)) < 0)
            return -1;
        return 0;
    }
    return -1;
}

ssize_t file_read(struct file *f, size_t addr, size_t n)
{
    ssize_t read_bytes = 0;

    if (f->readable == 0) return -1;

    if (f->type == FD_PIPE)
    {
        read_bytes = pipe_read(f->pipe, addr, n);
    }
    else if (f->type == FD_DEVICE)
    {
        if (f->major < 0 || f->major >= NDEV || !devsw[f->major].read)
            return -1;
        read_bytes = devsw[f->major].read(1, addr, n);
    }
    else if (f->type == FD_INODE)
    {
        inode_lock(f->ip);
        if ((read_bytes = inode_read(f->ip, true, addr, f->off, n)) > 0)
            f->off += read_bytes;
        inode_unlock(f->ip);
    }
    else
    {
        panic("file_read");
    }

    return read_bytes;
}

ssize_t file_write(struct file *f, size_t addr, size_t n)
{
    ssize_t r, ret = 0;

    if (f->writable == 0) return -1;

    if (f->type == FD_PIPE)
    {
        ret = pipe_write(f->pipe, addr, n);
    }
    else if (f->type == FD_DEVICE)
    {
        if (f->major < 0 || f->major >= NDEV || !devsw[f->major].write)
            return -1;
        ret = devsw[f->major].write(1, addr, n);
    }
    else if (f->type == FD_INODE)
    {
        // write a few blocks at a time to avoid exceeding
        // the maximum log transaction size, including
        // i-node, indirect block, allocation blocks,
        // and 2 blocks of slop for non-aligned writes.
        // this really belongs lower down, since inode_write()
        // might be writing a device like the console.
        ssize_t max = ((MAXOPBLOCKS - 1 - 1 - 2) / 2) * BLOCK_SIZE;
        ssize_t i = 0;
        while (i < n)
        {
            ssize_t n1 = n - i;
            if (n1 > max) n1 = max;

            log_begin_fs_transaction();
            inode_lock(f->ip);
            if ((r = inode_write(f->ip, true, addr + i, f->off, n1)) > 0)
                f->off += r;
            inode_unlock(f->ip);
            log_end_fs_transaction();

            if (r != n1)
            {
                // error from inode_write
                break;
            }
            i += r;
        }
        ret = (i == n ? n : -1);
    }
    else
    {
        panic("file_write");
    }

    return ret;
}
