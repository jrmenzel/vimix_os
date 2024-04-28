/* SPDX-License-Identifier: MIT */

// init: The initial user-level program

#include <kernel/major.h>

#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

char *argv[] = {"sh", 0};

int main()
{
    // init is called from the kernels initcode and has no open files.
    // The first three files are defined to be stdin, stdout and stderr.
    // This opens the console to be these standart files.
    // Note that fork() and execv() below wont change the open files,
    // this way all programs that don't change these open files will
    // direct all stdin/stdout/stderr IO to console.

    if (open("console", O_RDWR) < 0)
    {
        mknod("console", S_IFCHR | 0666,
              MKDEV(CONSOLE_DEVICE_MAJOR, CONSOLE_DEVICE_MINOR));
        open("console", O_RDWR);
    }
    dup(0);  // stdout
    dup(0);  // stderr

    while (true)
    {
        printf("init: starting sh\n");
        pid_t pid = fork();

        if (pid < 0)
        {
            printf("init: fork failed\n");
            exit(1);
        }
        if (pid == 0)
        {
            execv("sh", argv);
            printf("init: execv sh failed\n");
            exit(1);
        }

        while (true)
        {
            // this call to wait() returns if the shell exits,
            // or if a parentless process exits.
            int32_t status = 0;
            pid_t wpid = wait(&status);
            if (wpid == pid)
            {
                // the shell exited; restart it.
                printf("shell exited with status %d\n", status);
                break;
            }
            else if (wpid < 0)
            {
                printf("init: wait returned an error\n");
                exit(1);
            }
            else
            {
                // it was a parentless process; do nothing.
            }
        }
    }
}
