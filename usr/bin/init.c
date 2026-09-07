/* SPDX-License-Identifier: MIT */

// init: The initial user-level program

#include <kernel/major.h>

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <termios.h>
#include <unistd.h>

#define CONSOLE_MAX_PATH 16

struct Console
{
    char device[CONSOLE_MAX_PATH];
    pid_t pid;
    struct termios original_termios;
    bool have_original_termios;
    struct Console *next;
};

bool g_autologin_executed = false;

static int launch_login(struct Console *console)
{
    // The parent must keep track if autologin was triggered before
    // do this before the fork to pass on if the child should skip
    // the manual login.
    // This way the first login in console0 just works which is required for
    // automated tests and is convenient for manual testing while still
    // getting a login after the first shell exit.
    bool enable_autologin = ((strcmp(console->device, "/dev/console0") == 0) &&
                             (g_autologin_executed == false));
    if (enable_autologin)
    {
        g_autologin_executed = true;
    }

    console->pid = fork();
    if (console->pid < 0)
    {
        fprintf(stderr, "init: fork for %s failed: %s\n", console->device,
                strerror(errno));
        return -errno;
    }

    if (console->pid == 0)
    {
        // launch new console

        // redirect IO:
        close(STDIN_FILENO);
        close(STDOUT_FILENO);
        close(STDERR_FILENO);
        int stdin_fd = open(console->device, O_RDWR);
        if (stdin_fd < 0)
        {
            exit(1);
        }
        int stdout_fd = dup(0);  // stdout
        int stderr_fd = dup(0);  // stderr
        if ((stdin_fd != STDIN_FILENO) || (stdout_fd != STDOUT_FILENO) ||
            (stderr_fd != STDERR_FILENO))
        {
            exit(1);
        }

        if (console->have_original_termios &&
            tcsetattr(STDIN_FILENO, TCSAFLUSH, &console->original_termios) < 0)
        {
            fprintf(stderr, "init: failed to reset %s: %s\n", console->device,
                    strerror(errno));
        }

        // start login:
        const char *login_path = "/usr/bin/login";
        char *login_argv[] = {"login", enable_autologin ? "--autologin" : NULL,
                              NULL};
        printf("init starting %s\n", login_path);
        execv(login_path, login_argv);
        fprintf(stderr, "init: execv %s failed, %s\n", login_path,
                strerror(errno));
        exit(1);
    }

    return 0;
}

static bool is_console_name(const char *name)
{
    if (strncmp(name, "console", 7) != 0 || name[7] == '\0') return false;

    for (const char *p = name + 7; *p != '\0'; p++)
    {
        if (*p < '0' || *p > '9') return false;
    }
    return true;
}

static struct Console *enumerate_consoles(void)
{
    DIR *dir = opendir("/dev");
    if (dir == NULL)
    {
        return NULL;
    }

    struct Console *consoles = NULL;

    struct dirent *dir_entry = NULL;
    while ((dir_entry = readdir(dir)))
    {
        // for all entries:
        if (is_console_name(dir_entry->d_name))
        {
            struct Console *new_console = malloc(sizeof(struct Console));
            if (new_console == NULL)
            {
                printf("init: out of memory\n");
                closedir(dir);
                return consoles;
            }
            new_console->pid = 0;
            new_console->have_original_termios = false;
            new_console->next = consoles;
            consoles = new_console;
            int length = snprintf(new_console->device, CONSOLE_MAX_PATH,
                                  "/dev/%s", dir_entry->d_name);
            if (length < 0 || length >= CONSOLE_MAX_PATH)
            {
                consoles = new_console->next;
                free(new_console);
                continue;
            }

            int console_fd = open(new_console->device, O_RDWR);
            if (console_fd >= 0)
            {
                new_console->have_original_termios =
                    tcgetattr(console_fd, &new_console->original_termios) == 0;
                close(console_fd);
            }
        }
    }
    closedir(dir);

    return consoles;
}

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
    int stdin_fd = open("/dev/console0", O_RDWR);
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

    struct Console *consoles = enumerate_consoles();
    if (consoles == NULL)
    {
        fprintf(stderr, "init: no consoles found\n");
        exit(1);
    }

    for (struct Console *con = consoles; con != NULL; con = con->next)
    {
        launch_login(con);
    }

    while (true)
    {
        // this call to wait() returns if a login/shell exits,
        // or if a parentless process exits.
        int32_t status = 0;
        pid_t wpid = wait(&status);
        if (wpid < 0)
        {
            fprintf(stderr, "init: wait returned an error: %s\n",
                    strerror(errno));
            exit(1);
        }

        for (struct Console *con = consoles; con != NULL; con = con->next)
        {
            if (wpid == con->pid)
            {
                // The login exited; restart it. A short delay also prevents a
                // broken login binary/configuration from causing a fork loop.
                con->pid = 0;
                sleep(1);
                launch_login(con);
                break;
            }
        }

        // Otherwise it was a parentless process; do nothing.
    }
}
