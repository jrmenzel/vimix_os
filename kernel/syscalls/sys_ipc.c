/* SPDX-License-Identifier: MIT */
/*
 * Based on xv6.
 */

//
// Inter process communication system calls.
//
#include <syscalls/syscall.h>

#include <ipc/pipe.h>
#include <kernel/file.h>
#include <kernel/kernel.h>
#include <kernel/proc.h>

uint64 sys_pipe(void)
{
    uint64 fdarray;  // user pointer to array of two integers
    struct file *rf, *wf;
    int fd0, fd1;
    struct proc *p = myproc();

    argaddr(0, &fdarray);
    if (pipealloc(&rf, &wf) < 0) return -1;
    fd0 = -1;
    if ((fd0 = fdalloc(rf)) < 0 || (fd1 = fdalloc(wf)) < 0)
    {
        if (fd0 >= 0) p->ofile[fd0] = 0;
        fileclose(rf);
        fileclose(wf);
        return -1;
    }
    if (copyout(p->pagetable, fdarray, (char *)&fd0, sizeof(fd0)) < 0 ||
        copyout(p->pagetable, fdarray + sizeof(fd0), (char *)&fd1,
                sizeof(fd1)) < 0)
    {
        p->ofile[fd0] = 0;
        p->ofile[fd1] = 0;
        fileclose(rf);
        fileclose(wf);
        return -1;
    }
    return 0;
}
