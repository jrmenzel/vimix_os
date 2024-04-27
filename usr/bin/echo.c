/* SPDX-License-Identifier: MIT */

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int main(int argc, char *argv[])
{
    for (size_t i = 1; i < argc; i++)
    {
        write(STDOUT_FILENO, argv[i], strlen(argv[i]));
        if (i + 1 < argc)
        {
            write(STDOUT_FILENO, " ", 1);
        }
        else
        {
            write(STDOUT_FILENO, "\n", 1);
        }
    }

    return 0;
}
