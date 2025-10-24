/* SPDX-License-Identifier: MIT */

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

int main(int argc, char **argv)
{
    if (argc != 3)
    {
        fprintf(stderr, "Usage: %s <mode> <file>\n", argv[0]);
        return EXIT_FAILURE;
    }
    mode_t mode = (mode_t)strtoul(argv[1], NULL, 8);
    const char *path = argv[2];

    if (chmod(path, mode) < 0)
    {
        fprintf(stderr, "chmod failed: %s\n", strerror(errno));
        return EXIT_FAILURE;
    }

    return 0;
}
