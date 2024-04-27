/* SPDX-License-Identifier: MIT */

#include <kernel/kernel.h>
#include <kernel/stat.h>
#include <user.h>

int main(int argc, char **argv)
{
    if (argc < 2)
    {
        fprintf(2, "usage: kill pid...\n");
        exit(1);
    }
    for (size_t i = 1; i < argc; i++)
    {
        kill(atoi(argv[i]));
    }

    exit(0);
}
