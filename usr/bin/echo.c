/* SPDX-License-Identifier: MIT */

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int main(int argc, char *argv[])
{
    for (size_t i = 1; i < argc; i++)
    {
        size_t str_len = strlen(argv[i]);
        ssize_t written = write(STDOUT_FILENO, argv[i], str_len);
        if (written != str_len) return -1;

        if (i + 1 < argc)
        {
            written = write(STDOUT_FILENO, " ", 1);
            if (written != 1) return -1;
        }
        else
        {
            written = write(STDOUT_FILENO, "\n", 1);
            if (written != 1) return -1;
        }
    }

    return 0;
}
