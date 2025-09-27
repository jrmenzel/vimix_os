/* SPDX-License-Identifier: MIT */

#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <vimixutils/sysfs.h>

size_t get_from_sysfs(const char *path)
{
    int fd = open(path, O_RDONLY);
    if (fd < 0)
    {
        fprintf(stderr, "open of %s failed: %s (%d)\n", path, strerror(errno),
                errno);
        exit(-1);
    }

    const size_t BUF_SIZE = 128;
    char buf[BUF_SIZE];
    if (read(fd, buf, sizeof(buf)) < 0)
    {
        fprintf(stderr, "read of %s failed: %s (%d)\n", path, strerror(errno),
                errno);
        close(fd);
        exit(-1);
    }
    close(fd);
    buf[BUF_SIZE - 1] = 0;  // ensure 0-termination

    return strtoul(buf, NULL, 10);
}
