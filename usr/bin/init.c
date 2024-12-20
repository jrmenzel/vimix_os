/* SPDX-License-Identifier: MIT */

// init: The initial user-level program

#include <kernel/major.h>

#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

int make_dev(const char *file, int device_type, dev_t dev)
{
    int f = open(file, O_RDWR);
    if (f < 0)
    {
        return mknod(file, device_type | 0666, dev);
    }
    close(f);

    return 0;
}

int main()
{
    // init is called from the kernels initcode and has no open files.
    // The first three files are defined to be stdin, stdout and stderr.
    // This opens the console to be these standart files.
    // Note that fork() and execv() below wont change the open files,
    // this way all programs that don't change these open files will
    // direct all stdin/stdout/stderr IO to console.

    if (open("/dev/console", O_RDWR) < 0)
    {
        mknod("/dev/console", S_IFCHR | 0666, MKDEV(CONSOLE_DEVICE_MAJOR, 0));
        open("/dev/console", O_RDWR);
    }
    dup(0);  // stdout
    dup(0);  // stderr

    make_dev("/dev/null", S_IFCHR, MKDEV(DEV_NULL_MAJOR, 0));
    make_dev("/dev/zero", S_IFCHR, MKDEV(DEV_ZERO_MAJOR, 0));

    while (true)
    {
        const char *SHELL_PATH = "/usr/bin/sh";
        printf("init: starting %s\n", SHELL_PATH);
        pid_t pid = fork();

        if (pid < 0)
        {
            printf("init: fork failed\n");
            exit(1);
        }
        if (pid == 0)
        {
            // HACK to enable automated testing:
            // if /tests/autoexec.sh exists, run it. It should contain a system
            // shutdown.
            struct stat st;
            char *SHELL_ARGV[] = {"sh", 0};
            char *SHELL_ARGV_AUTORUN[] = {"sh", "/tests/autoexec.sh", 0};
            if (stat(SHELL_ARGV_AUTORUN[1], &st) == 0)
            {
                execv(SHELL_PATH, SHELL_ARGV_AUTORUN);
            }
            else
            {
                // default path, just start a shell:
                execv(SHELL_PATH, SHELL_ARGV);
            }

            printf("init: execv sh failed\n");
            exit(1);
        }

        while (true)
        {
            // this call to wait() returns if the shell exits,
            // or if a parentless process exits.
            int32_t status = 0;
            pid_t wpid = wait(&status);
            status = WEXITSTATUS(status);
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
