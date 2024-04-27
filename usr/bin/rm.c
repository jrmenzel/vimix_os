/* SPDX-License-Identifier: MIT */

#include <kernel/kernel.h>
#include <kernel/stat.h>
#include <user.h>

int main(int argc, char *argv[])
{
    if (argc < 2)
    {
        fprintf(2, "Usage: rm files...\n");
        exit(1);
    }

    for (size_t i = 1; i < argc; i++)
    {
        if (unlink(argv[i]) < 0)
        {
            fprintf(2, "rm: %s failed to delete\n", argv[i]);
            break;
        }
    }

    exit(0);
}
