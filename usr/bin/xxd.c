/* SPDX-License-Identifier: MIT */

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <vimixutils/minmax.h>

///
/// Temp buffer for read/write in chunks.
///
char buf[512];
const size_t bytes_per_line = 16;

size_t write_hex_line(char *buf, size_t n, size_t index)
{
    printf("%08zx: ", index);
    for (size_t i = 0; i < n; ++i)
    {
        printf("%02x", (uint8_t)buf[i]);
        if (i % 2 == 1) printf(" ");
    }
    for (size_t i = n; i < bytes_per_line; ++i)
    {
        printf("  ");
        if (i % 2 == 1) printf(" ");
    }

    printf(" ");

    for (size_t i = 0; i < n; ++i)
    {
        if (isprint(buf[i]))
        {
            printf("%c", (uint8_t)buf[i]);
        }
        else
        {
            printf(".");
        }
    }

    printf("\n");
    return n;
}

size_t write_hex(char *buf, size_t n, size_t index)
{
    size_t written = 0;
    for (size_t i = 0; i < n; i += bytes_per_line)
    {
        size_t to_write = (n - written);
        to_write = min(to_write, bytes_per_line);

        written += write_hex_line(buf + i, to_write, index + written);
    }
    return n;
}

///
/// Read data from fd and dump the hex values
///
void xxd(int fd, size_t len)
{
    int n = 0;
    size_t index = 0;

    while (true)
    {
        size_t to_read = min(sizeof(buf), (len - index));
        if (to_read == 0) break;

        n = read(fd, buf, to_read);
        if (n <= 0) break;

        if (write_hex(buf, n, index) != n)
        {
            fprintf(stderr, "xxd: write error (errno: %s)\n", strerror(errno));
            exit(1);
        }
        index += n;
    }
    if (n < 0)
    {
        fprintf(stderr, "xxd: read error (errno: %s)\n", strerror(errno));
        exit(1);
    }
}

void print_usage()
{
    printf("usage: xxd <file>\n");
    printf("       xxd -l <byte count> <file>\n");
}

/// @brief hex dump of a file
/// @return 0 on success
int main(int argc, char *argv[])
{
    if (argc <= 1)
    {
        print_usage();
        return -1;
    }

    size_t file_index = 1;
    size_t len = (-1);
    if (strcmp(argv[1], "-l") == 0)
    {
        ssize_t len_signed = atoi(argv[2]);
        if (len_signed > 0)
        {
            len = len_signed;
        }
        file_index += 2;
    }

    if (argc <= file_index)
    {
        print_usage();
        return -1;
    }
    int fd;
    if ((fd = open(argv[file_index], O_RDONLY)) < 0)
    {
        fprintf(stderr, "xxd: cannot open %s\n", argv[file_index]);
        return -1;
    }
    xxd(fd, len);
    close(fd);

    return 0;
}
