/* SPDX-License-Identifier: MIT */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>
#include <unistd.h>

int main(int argc, char *argv[])
{
    if (argc < 2)
    {
        fprintf(stderr, "Usage: stat file\n");
        return 1;
    }

    struct stat st;
    if (stat(argv[1], &st) < 0)
    {
        fprintf(stderr, "stat: file or directory %s not found\n", argv[1]);
        return 1;
    }

    printf("  File: %s\n", argv[1]);
    printf("  Size: %zd   Blocks: %zd   IO Blocks: %zd   ", st.st_size,
           st.st_blocks, st.st_blksize);

    if (S_ISDIR(st.st_mode))
    {
        printf("directory\n");
    }
    else if (S_ISREG(st.st_mode))
    {
        printf("regular file\n");
    }
    else if (S_ISCHR(st.st_mode))
    {
        printf("character special file\n");
    }
    else if (S_ISBLK(st.st_mode))
    {
        printf("block special file\n");
    }

    printf("Device: %d,%d   Inode: %ld   Links: %ld   ", major(st.st_dev),
           minor(st.st_dev), st.st_ino, (unsigned long)st.st_nlink);
    if (S_ISCHR(st.st_mode) || S_ISBLK(st.st_mode))
    {
        printf("Device type: %d,%d", major(st.st_rdev), minor(st.st_rdev));
    }
    printf("\n");

    return 0;
}
