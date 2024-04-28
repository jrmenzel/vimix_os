/* SPDX-License-Identifier: MIT */

//
// Inter process communication system calls.
//
#include <syscalls/syscall.h>

#include <ipc/pipe.h>
#include <kernel/file.h>
#include <kernel/kernel.h>
#include <kernel/proc.h>

size_t sys_pipe()
{
    // parameter 0: int pipe_descriptors[]
    size_t fdarray;
    argaddr(0, &fdarray);

    struct file *rf, *wf;
    if (pipe_alloc(&rf, &wf) < 0)
    {
        return -1;
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
        return -1;
    }
    if (uvm_copy_out(proc->pagetable, fdarray, (char *)&fd0, sizeof(fd0)) < 0 ||
        uvm_copy_out(proc->pagetable, fdarray + sizeof(fd0), (char *)&fd1,
                     sizeof(fd1)) < 0)
    {
        proc->files[fd0] = NULL;
        proc->files[fd1] = NULL;
        file_close(rf);
        file_close(wf);
        return -1;
    }
    return 0;
}
