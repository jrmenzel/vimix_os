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
    size_t first_arg_to_echo = 1;
    bool newline = true;

    if ((argc > 1) && (strcmp(argv[1], "-n") == 0))
    {
        first_arg_to_echo = 2;
        newline = false;
    }

    for (size_t i = first_arg_to_echo; i < argc; i++)
    {
        size_t str_len = strlen(argv[i]);
        write_or_quit(STDOUT_FILENO, argv[i], str_len);

        if (i + 1 < argc)
        {
            write_or_quit(STDOUT_FILENO, " ", 1);
        }
        else if (newline)
        {
            write_or_quit(STDOUT_FILENO, "\n", 1);
        }
    }

    return 0;
}
