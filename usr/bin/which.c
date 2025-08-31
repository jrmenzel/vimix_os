/* SPDX-License-Identifier: MIT */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <vimixutils/path.h>

int main(int argc, char *argv[])
{
    if (argc != 2)
    {
        fprintf(stderr, "Usage: which [command]\n");
        return 1;
    }

    char *binary_path = find_program_in_path(argv[1]);
    if (binary_path == NULL)
    {
        fprintf(stderr, "no %s in search path\n", argv[1]);
        return 1;
    }

    printf("%s\n", binary_path);
    free(binary_path);

    return 0;
}
