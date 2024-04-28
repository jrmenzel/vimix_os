/* SPDX-License-Identifier: MIT */

#include <ipc/pipe.h>
#include <kernel/fcntl.h>
#include <kernel/file.h>
#include <kernel/fs.h>
#include <kernel/kalloc.h>
#include <kernel/kernel.h>
#include <kernel/proc.h>
#include <kernel/sleeplock.h>
#include <kernel/spinlock.h>
#include <kernel/vm.h>

static inline bool pipe_is_empty(struct pipe *pipe)
{
    return pipe->nread == pipe->nwrite;
}

static inline bool pipe_is_full(struct pipe *pipe)
{
    return pipe->nwrite == pipe->nread + PIPE_SIZE;
}

int32_t pipe_alloc(struct file **f0, struct file **f1)
{
    // create two files
    *f0 = file_alloc();
    *f1 = file_alloc();
    if (*f0 == NULL || *f1 == NULL)
    {
        if (*f0) file_close(*f0);
        if (*f1) file_close(*f1);
        return -1;
    }

    // create the pipe
    struct pipe *new_pipe = (struct pipe *)kalloc();
    if (new_pipe == NULL)
    {
        file_close(*f0);
        file_close(*f1);
        return -1;
    }

    // stay true till pipe_close() is called:
    new_pipe->read_open = true;
    new_pipe->write_open = true;

    new_pipe->nwrite = 0;
    new_pipe->nread = 0;
    spin_lock_init(&new_pipe->lock, "pipe");

    // read end
    (*f0)->mode = S_IFIFO | S_IRUSR;
    (*f0)->flags = O_RDONLY;
    (*f0)->pipe = new_pipe;

    // write end
    (*f1)->mode = S_IFIFO | S_IWUSR;
    (*f1)->flags = O_WRONLY;
    (*f1)->pipe = new_pipe;

    return 0;
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

    // free if both ends closed the pipe
    bool free_pipe = (!pipe->read_open) && (!pipe->write_open);
    spin_unlock(&pipe->lock);

    if (free_pipe)
    {
        kfree((void *)pipe);
    }
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

        if (pipe_is_full(pipe))
        {
            wakeup(&pipe->nread);
            // wait till another process read from the pipe
            sleep(&pipe->nwrite, &pipe->lock);
        }
        else
        {
            char ch;
            if (uvm_copy_in(proc->pagetable, &ch, src_user_addr + i, 1) == -1)
            {
                break;
            }
            pipe->data[pipe->nwrite % PIPE_SIZE] = ch;
            pipe->nwrite++;
            i++;
        }
    }
    wakeup(&pipe->nread);
    spin_unlock(&pipe->lock);

    return i;
}

ssize_t pipe_read(struct pipe *pipe, size_t src_kernel_addr, size_t n)
{
    struct process *proc = get_current();

    spin_lock(&pipe->lock);
    while (pipe_is_empty(pipe) && pipe->write_open)
    {
        if (proc_is_killed(proc))
        {
            spin_unlock(&pipe->lock);
            return -1;
        }
        // wait for another process to write into the pipe
        sleep(&pipe->nread, &pipe->lock);
    }

    size_t i;
    for (i = 0; i < n; i++)
    {
        if (pipe_is_empty(pipe))
        {
            break;
        }

        char ch = pipe->data[pipe->nread % PIPE_SIZE];
        pipe->nread++;

        if (uvm_copy_out(proc->pagetable, src_kernel_addr + i, &ch, 1) == -1)
        {
            break;
        }
    }
    wakeup(&pipe->nwrite);
    spin_unlock(&pipe->lock);

    return i;
}
