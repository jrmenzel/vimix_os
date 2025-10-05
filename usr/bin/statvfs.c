/* SPDX-License-Identifier: MIT */

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/statvfs.h>
#include <unistd.h>

int main(int argc, char *argv[])
{
    if (argc < 2)
    {
        fprintf(stderr, "Usage: statvfs file\n");
        return 1;
    }

    struct statvfs stat_fs;
    if (statvfs(argv[1], &stat_fs) < 0)
    {
        fprintf(stderr, "statvfs: %s\n", strerror(errno));
        return 1;
    }

    printf("f_bsize:   %zu\n", stat_fs.f_bsize);
    printf("f_frsize:  %zu\n", stat_fs.f_frsize);
    printf("f_blocks:  %zu\n", stat_fs.f_blocks);
    printf("f_bfree:   %zu\n", stat_fs.f_bfree);
    printf("f_bavail:  %zu\n", stat_fs.f_bavail);
    printf("f_files:   %zu\n", stat_fs.f_files);
    printf("f_ffree:   %zu\n", stat_fs.f_ffree);
    printf("f_favail:  %zu\n", stat_fs.f_favail);
    printf("f_fsid:    %zu\n", stat_fs.f_fsid);
    printf("f_flag:    %zu\n", stat_fs.f_flag);
    printf("f_namemax: %zu\n", stat_fs.f_namemax);

    return 0;
}
