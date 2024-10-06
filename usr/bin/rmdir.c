/* SPDX-License-Identifier: MIT */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int main(int argc, char *argv[])
{
    if (argc < 2)
    {
        fprintf(stderr, "Usage: rmdir directories...\n");
        return 1;
    }

    for (size_t i = 1; i < argc; i++)
    {
        if (rmdir(argv[i]) < 0)
        {
            fprintf(stderr, "rmdir: %s failed to delete\n", argv[i]);
            return 1;
        }
    }

    return 0;
}
