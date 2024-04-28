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

    struct stat buffer;
    if (stat(argv[1], &buffer) < 0)
    {
        fprintf(stderr, "stat: file or directory %s not found\n", argv[1]);
        return 1;
    }

    if (S_ISDIR(buffer.st_mode))
    {
        printf("Directory: %s\n", argv[1]);
    }
    else if (S_ISREG(buffer.st_mode))
    {
        printf("File: %s\n", argv[1]);
    }
    else if (S_ISCHR(buffer.st_mode))
    {
        printf("Char device: %s\n", argv[1]);
    }
    else if (S_ISBLK(buffer.st_mode))
    {
        printf("Block device: %s\n", argv[1]);
    }

    printf("Size:      %d\n", (int)buffer.st_size);
    printf("Device: (%d,%d)\n", major(buffer.st_dev), minor(buffer.st_dev));
    printf("rdev:   (%d,%d)\n", major(buffer.st_rdev), minor(buffer.st_rdev));
    printf("Inode:     %d\n", (int)buffer.st_ino);
    printf("Links:     %d\n", (int)buffer.st_nlink);

    return 0;
}
