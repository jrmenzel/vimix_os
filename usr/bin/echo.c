/* SPDX-License-Identifier: MIT */

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

void write_or_quit(int fd, const char *buf, size_t n)
{
    ssize_t written = write(fd, buf, n);
    if (written != n)
    {
        fprintf(stderr, "echo: write error, errno %s\n", strerror(errno));
        exit(-1);
    }
}

int main(int argc, char *argv[])
{
    for (size_t i = 1; i < argc; i++)
    {
        size_t str_len = strlen(argv[i]);
        write_or_quit(STDOUT_FILENO, argv[i], str_len);

        if (i + 1 < argc)
        {
            write_or_quit(STDOUT_FILENO, " ", 1);
        }
        else
        {
            write_or_quit(STDOUT_FILENO, "\n", 1);
        }
    }

    return 0;
}
