/* SPDX-License-Identifier: MIT */

#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#if defined(BUILD_ON_HOST)
#include <linux/limits.h>
#else
#include <kernel/limits.h>
#endif

#define BUFFER_SIZE 512
char io_buffer[BUFFER_SIZE];

int copy(char *from, char *to);

int main(int argc, char *argv[])
{
    if (argc != 3)
    {
        fprintf(stderr, "Usage: cp from to\n");
        return 1;
    }

    return copy(argv[1], argv[2]);
}

int copy(char *from, char *to)
{
    int fd_from = open(from, O_RDONLY);
    if (fd_from < 0)
    {
        fprintf(stderr, "cp: cannot open %s (errno: %d)\n", from, errno);
        return 1;
    }

    struct stat stbuf1, stbuf2;
    if (fstat(fd_from, &stbuf1) < 0)
    {
        fprintf(stderr, "cp: cannot stat %s (errno: %d)\n", from, errno);
        return 1;
    }
    int stat2_res = stat(to, &stbuf2);
    mode_t mode = stbuf1.st_mode;

    char to_path[PATH_MAX];
    strncpy(to_path, to, PATH_MAX);

    if ((stat2_res == 0) && ((stbuf2.st_mode & S_IFMT) == S_IFDIR))
    {
        // to is a directory: copy to "to/from"
        strncpy(to_path, to, PATH_MAX);
        size_t dir_len = strnlen(to, PATH_MAX);
        if (dir_len < (PATH_MAX - 1) && to_path[dir_len - 1] != '/')
        {
            // "to"+'/' if there was no '/' already
            to_path[dir_len] = '/';
            dir_len++;
        }
        strncpy(&to_path[dir_len], from, PATH_MAX - dir_len);
    }

    if ((stat2_res == 0) &&
        ((stbuf1.st_dev == stbuf2.st_dev) && (stbuf1.st_ino == stbuf2.st_ino)))
    {
        fprintf(stderr, "cp: cannot copy file to itself.\n");
        return 1;
    }

    int fd_to = creat(to_path, mode);
    if (fd_to < 0)
    {
        fprintf(stderr, "cp: cannot create %s\n", to_path);
        close(fd_from);
        return 1;
    }

    ssize_t n;
    while ((n = read(fd_from, io_buffer, BUFFER_SIZE)) != 0)
    {
        if (n < 0)
        {
            fprintf(stderr, "cp: read error\n");
            close(fd_from);
            close(fd_to);
            return 1;
        }
        else if (write(fd_to, io_buffer, n) != n)
        {
            fprintf(stderr, "cp: write error.\n");
            close(fd_from);
            close(fd_to);
            return 1;
        }
    }
    close(fd_from);
    close(fd_to);
    return 0;
}
