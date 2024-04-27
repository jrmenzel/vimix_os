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
    size_t fdarray;  // user pointer to array of two integers
    struct file *rf, *wf;
    int fd0, fd1;
    struct process *proc = get_current();

    argaddr(0, &fdarray);
    if (pipe_alloc(&rf, &wf) < 0) return -1;
    fd0 = -1;
    if ((fd0 = fd_alloc(rf)) < 0 || (fd1 = fd_alloc(wf)) < 0)
    {
        if (fd0 >= 0) proc->files[fd0] = 0;
        file_close(rf);
        file_close(wf);
        return -1;
    }
    if (uvm_copy_out(proc->pagetable, fdarray, (char *)&fd0, sizeof(fd0)) < 0 ||
        uvm_copy_out(proc->pagetable, fdarray + sizeof(fd0), (char *)&fd1,
                     sizeof(fd1)) < 0)
    {
        proc->files[fd0] = 0;
        proc->files[fd1] = 0;
        file_close(rf);
        file_close(wf);
        return -1;
    }
    return 0;
}
