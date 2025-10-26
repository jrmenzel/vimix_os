/* SPDX-License-Identifier: MIT */

// init: The initial user-level program

#include <kernel/major.h>

#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <termios.h>
#include <unistd.h>

int main()
{
    // init is executed from the kernel explicitly and has no open files.
    // The first three files are defined to be stdin, stdout and stderr.
    // This opens the console to be these standart files.
    // Note that fork() and execv() below wont change the open files,
    // this way all programs that don't change these open files will
    // direct all stdin/stdout/stderr IO to console.

    int ret = mount("dev", "/dev", "devfs", 0, NULL);
    if (ret < 0) return -errno;

    umask(022);  // default umask for init
    int stdin_fd = open("/dev/console", O_RDWR);
    if (stdin_fd < 0)
    {
        return -errno;
    }
    int stdout_fd = dup(0);  // stdout
    int stderr_fd = dup(0);  // stderr
    if ((stdin_fd != STDIN_FILENO) || (stdout_fd != STDOUT_FILENO) ||
        (stderr_fd != STDERR_FILENO))
    {
        return -errno;
    }

    // wait till print works:
    printf("init mounting /dev... OK\n");

    // mount /sys
    ret = mount("sys", "/sys", "sysfs", 0, NULL);
    if (ret < 0)
    {
        fprintf(stderr, "init mounting /sys failed. Error %s\n",
                strerror(errno));
    }
    else
    {
        printf("init mounting /sys... OK\n");
    }

    struct termios original_termios;
    tcgetattr(STDIN_FILENO, &original_termios);

    int fd_dev = open("/dev/virtio1", O_RDONLY);
    if (fd_dev >= 0)
    {
        close(fd_dev);
        printf("init mounting /home... ");
        int ret = mount("/dev/virtio1", "/home", "vimixfs", 0, NULL);
        if (ret < 0)
        {
            printf("failed. Error %d\n", errno);
        }
        else
        {
            printf("OK\n");
        }
    }

    while (true)
    {
        tcsetattr(STDIN_FILENO, TCSAFLUSH, &original_termios);

        pid_t pid = fork();
        if (pid < 0)
        {
            fprintf(stderr, "init: fork failed\n");
            exit(1);
        }
        if (pid == 0)
        {
            const char *login_path = "/usr/bin/login";
            char *login_argv[] = {"login", 0};
            printf("init starting %s\n", login_path);
            if (execv(login_path, login_argv) < 0)
            {
                fprintf(stderr, "init: execv %s failed, errno: %s\n",
                        login_path, strerror(errno));
            }

            fprintf(stderr, "init: execv %s failed\n", login_path);
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
                // the login exited; restart it.
                // printf("login exited with status %d\n", status);
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
