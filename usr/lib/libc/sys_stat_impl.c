/* SPDX-License-Identifier: MIT */

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

int32_t stat(const char *path, struct stat *buffer)
{
    FILE_DESCRIPTOR fd = open(path, O_RDONLY);
    if (fd < 0)
    {
        return -1;
    }

    int32_t r = fstat(fd, buffer);
    close(fd);
    return r;
}
