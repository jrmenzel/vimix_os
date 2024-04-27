/* SPDX-License-Identifier: MIT */

#include <kernel/kernel.h>
#include <kernel/stat.h>
#include <user.h>

int main(int argc, char *argv[])
{
    for (size_t i = 1; i < argc; i++)
    {
        write(1, argv[i], strlen(argv[i]));
        if (i + 1 < argc)
        {
            write(1, " ", 1);
        }
        else
        {
            write(1, "\n", 1);
        }
    }
    exit(0);
}
