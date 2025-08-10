/* SPDX-License-Identifier: MIT */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>

int main(int argc, char *argv[])
{
    if (argc < 2)
    {
        fprintf(stderr, "Usage: mkdir files...\n");
        return 1;
    }

    int some_dirs_failed = 0;
    for (size_t i = 1; i < argc; i++)
    {
        if (mkdir(argv[i], 0755) < 0)
        {
            fprintf(stderr, "mkdir: %s failed to create\n", argv[i]);
            some_dirs_failed = 1;
        }
    }

    return some_dirs_failed;
}
