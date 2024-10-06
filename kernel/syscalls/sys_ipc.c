/* SPDX-License-Identifier: MIT */

//
// Inter process communication system calls.
//
#include <syscalls/syscall.h>

#include <ipc/pipe.h>
#include <kernel/file.h>
#include <kernel/kernel.h>
#include <kernel/proc.h>

ssize_t sys_pipe()
{
    // parameter 0: int pipe_descriptors[]
    size_t fdarray;
    argaddr(0, &fdarray);

    struct file *rf, *wf;
    ssize_t ret = pipe_alloc(&rf, &wf);
    if (ret < 0)
    {
        return ret;
    }

    struct process *proc = get_current();
    FILE_DESCRIPTOR fd0 = INVALID_FILE_DESCRIPTOR;
    FILE_DESCRIPTOR fd1 = INVALID_FILE_DESCRIPTOR;
    if ((fd0 = fd_alloc(rf)) < 0 || (fd1 = fd_alloc(wf)) < 0)
    {
        if (fd0 >= 0)
        {
            proc->files[fd0] = NULL;
        }
        file_close(rf);
        file_close(wf);
        return -EMFILE;
    }
    if (uvm_copy_out(proc->pagetable, fdarray, (char *)&fd0, sizeof(fd0)) < 0 ||
        uvm_copy_out(proc->pagetable, fdarray + sizeof(fd0), (char *)&fd1,
                     sizeof(fd1)) < 0)
    {
        proc->files[fd0] = NULL;
        proc->files[fd1] = NULL;
        file_close(rf);
        file_close(wf);
        return -EFAULT;
    }
    return 0;
}
