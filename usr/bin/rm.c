/* SPDX-License-Identifier: MIT */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int main(int argc, char *argv[])
{
    if (argc < 2)
    {
        fprintf(stderr, "Usage: rm files...\n");
        return 1;
    }

    for (size_t i = 1; i < argc; i++)
    {
        if (unlink(argv[i]) < 0)
        {
            fprintf(stderr, "rm: %s failed to delete\n", argv[i]);
            return 1;
        }
    }

    return 0;
}
