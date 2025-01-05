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

size_t fortune_offset[256];

int fortune(char *filename)
{
    int fd;

    if ((fd = open(filename, O_RDONLY)) < 0)
    {
        fprintf(stderr, "fortune: cannot open %s\n", filename);
        return 1;
    }

    size_t chunk = 0;
    size_t number_of_fortunes = 0;
    fortune_offset[number_of_fortunes++] = 0;

    int n;
    while ((n = read(fd, buf, sizeof(buf))) > 0)
    {
        for (size_t i = 0; i < n - 1; ++i)
        {
            if (buf[i] == '\n')
            {
                fortune_offset[number_of_fortunes++] =
                    (chunk * sizeof(buf)) + i + 1;
            }
        }
        chunk++;
    }
    if (n < 0)
    {
        fprintf(stderr, "fortune: read error (errno: %s)\n", strerror(errno));
        exit(1);
    }

    int fd_random;
    if ((fd_random = open("/dev/random", O_RDONLY)) < 0)
    {
        fprintf(stderr, "fortune: cannot open /dev/random\n");
        close(fd);
        return 1;
    }

    uint32_t random_value;
    read(fd_random, &random_value, sizeof(uint32_t));
    close(fd_random);
    random_value = random_value % number_of_fortunes;

    lseek(fd, fortune_offset[random_value], SEEK_SET);
    int bytes_read = read(fd, buf, sizeof(buf));
    for (size_t i = 0; i < bytes_read; ++i)
    {
        // terminate fortune string
        if (buf[i] == '\n')
        {
            buf[i] = 0;
            break;
        }
    }
    if (bytes_read < sizeof(buf)) buf[bytes_read] = 0;
    printf("%s\n", buf);

    close(fd);
    return 0;
}

int main(int argc, char *argv[])
{
    if (argc == 1)
    {
        return fortune("/etc/fortune");
    }
    else if (argc == 2)
    {
        return fortune(argv[1]);
    }
    else
    {
        printf("usage: fortune\n");
        printf("       fortune <fortune file>\n");
        return 1;
    }
}