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

int32_t pipe_alloc(struct file **f0, struct file **f1)
{
    struct pipe *pi = NULL;

    *f0 = *f1 = 0;
    if ((*f0 = file_alloc()) == 0 || (*f1 = file_alloc()) == 0) goto bad;
    if ((pi = (struct pipe *)kalloc()) == 0) goto bad;
    pi->read_open = 1;
    pi->write_open = 1;
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

void pipe_close(struct pipe *pipe, bool close_writing_end)
{
    spin_lock(&pipe->lock);
    if (close_writing_end)
    {
        pipe->write_open = false;
        wakeup(&pipe->nread);
    }
    else
    {
        pipe->read_open = false;
        wakeup(&pipe->nwrite);
    }
    if (pipe->read_open == false && pipe->write_open == false)
    {
        spin_unlock(&pipe->lock);
        kfree((char *)pipe);
    }
    else
        spin_unlock(&pipe->lock);
}

ssize_t pipe_write(struct pipe *pipe, size_t src_user_addr, size_t n)
{
    size_t i = 0;
    struct process *proc = get_current();

    spin_lock(&pipe->lock);
    while (i < n)
    {
        if (pipe->read_open == false || proc_is_killed(proc))
        {
            spin_unlock(&pipe->lock);
            return -1;
        }
        if (pipe->nwrite == pipe->nread + PIPE_SIZE)
        {
            wakeup(&pipe->nread);
            // wait till another process read from the pipe
            sleep(&pipe->nwrite, &pipe->lock);
        }
        else
        {
            char ch;
            if (uvm_copy_in(proc->pagetable, &ch, src_user_addr + i, 1) == -1)
                break;
            pipe->data[pipe->nwrite++ % PIPE_SIZE] = ch;
            i++;
        }
    }
    wakeup(&pipe->nread);
    spin_unlock(&pipe->lock);

    return i;
}

ssize_t pipe_read(struct pipe *pipe, size_t src_kernel_addr, size_t n)
{
    int i;
    struct process *proc = get_current();
    char ch;

    spin_lock(&pipe->lock);
    while (pipe->nread == pipe->nwrite && pipe->write_open)
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
        ch = pipe->data[pipe->nread++ % PIPE_SIZE];
        if (uvm_copy_out(proc->pagetable, src_kernel_addr + i, &ch, 1) == -1)
            break;
    }
    wakeup(&pipe->nwrite);
    spin_unlock(&pipe->lock);
    return i;
}
