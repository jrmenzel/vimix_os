/* SPDX-License-Identifier: MIT */

#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

///
/// Temp buffer for read/write in chunks.
///
char buf[512];

///
/// Read data from fd in larger chunks and write them to stdout
///
void cat(int fd)
{
    int n;

    while ((n = read(fd, buf, sizeof(buf))) > 0)
    {
        if (write(STDOUT_FILENO, buf, n) != n)
        {
            fprintf(stderr, "cat: write error (errno: %s)\n", strerror(errno));
            exit(1);
        }
    }
    if (n < 0)
    {
        fprintf(stderr, "cat: read error (errno: %s)\n", strerror(errno));
        exit(1);
    }
}

/// @brief concatenate files
/// @param argc Argument count
/// @param argv File names to concatenate to stdout
/// @return 0 on success
int main(int argc, char *argv[])
{
    if (argc <= 1)
    {
        cat(STDIN_FILENO);
        return 0;
    }

    for (size_t i = 1; i < argc; i++)
    {
        int fd;

        if ((fd = open(argv[i], O_RDONLY)) < 0)
        {
            fprintf(stderr, "cat: cannot open %s\n", argv[i]);
            return 1;
        }
        cat(fd);
        close(fd);
    }
    return 0;
}
