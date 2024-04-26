/* SPDX-License-Identifier: MIT */

#include <ipc/pipe.h>
#include <kernel/file.h>
#include <kernel/fs.h>
#include <kernel/kalloc.h>
#include <kernel/kernel.h>
#include <kernel/proc.h>
#include <kernel/sleeplock.h>
#include <kernel/spinlock.h>
#include <kernel/vm.h>

int pipe_alloc(struct file **f0, struct file **f1)
{
    struct pipe *pi;

    pi = 0;
    *f0 = *f1 = 0;
    if ((*f0 = file_alloc()) == 0 || (*f1 = file_alloc()) == 0) goto bad;
    if ((pi = (struct pipe *)kalloc()) == 0) goto bad;
    pi->readopen = 1;
    pi->writeopen = 1;
    pi->nwrite = 0;
    pi->nread = 0;
    spin_lock_init(&pi->lock, "pipe");
    (*f0)->type = FD_PIPE;
    (*f0)->readable = 1;
    (*f0)->writable = 0;
    (*f0)->pipe = pi;
    (*f1)->type = FD_PIPE;
    (*f1)->readable = 0;
    (*f1)->writable = 1;
    (*f1)->pipe = pi;
    return 0;

bad:
    if (pi) kfree((char *)pi);
    if (*f0) file_close(*f0);
    if (*f1) file_close(*f1);
    return -1;
}

void pipe_close(struct pipe *pipe, int writable)
{
    spin_lock(&pipe->lock);
    if (writable)
    {
        pipe->writeopen = 0;
        wakeup(&pipe->nread);
    }
    else
    {
        pipe->readopen = 0;
        wakeup(&pipe->nwrite);
    }
    if (pipe->readopen == 0 && pipe->writeopen == 0)
    {
        spin_unlock(&pipe->lock);
        kfree((char *)pipe);
    }
    else
        spin_unlock(&pipe->lock);
}

int pipe_write(struct pipe *pipe, uint64_t src_user_addr, int n)
{
    int i = 0;
    struct process *proc = get_current();

    spin_lock(&pipe->lock);
    while (i < n)
    {
        if (pipe->readopen == 0 || proc_is_killed(proc))
        {
            spin_unlock(&pipe->lock);
            return -1;
        }
        if (pipe->nwrite == pipe->nread + PIPESIZE)
        {
            wakeup(&pipe->nread);
            sleep(&pipe->nwrite, &pipe->lock);
        }
        else
        {
            char ch;
            if (uvm_copy_in(proc->pagetable, &ch, src_user_addr + i, 1) == -1)
                break;
            pipe->data[pipe->nwrite++ % PIPESIZE] = ch;
            i++;
        }
    }
    wakeup(&pipe->nread);
    spin_unlock(&pipe->lock);

    return i;
}

int pipe_read(struct pipe *pipe, uint64_t src_kernel_addr, int n)
{
    int i;
    struct process *proc = get_current();
    char ch;

    spin_lock(&pipe->lock);
    while (pipe->nread == pipe->nwrite && pipe->writeopen)
    {
        if (proc_is_killed(proc))
        {
            spin_unlock(&pipe->lock);
            return -1;
        }
        sleep(&pipe->nread, &pipe->lock);
    }
    for (i = 0; i < n; i++)
    {
        if (pipe->nread == pipe->nwrite) break;
        ch = pipe->data[pipe->nread++ % PIPESIZE];
        if (uvm_copy_out(proc->pagetable, src_kernel_addr + i, &ch, 1) == -1)
            break;
    }
    wakeup(&pipe->nwrite);
    spin_unlock(&pipe->lock);
    return i;
}
