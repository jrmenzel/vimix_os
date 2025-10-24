/* SPDX-License-Identifier: MIT */

#include <errno.h>
#include <grp.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int main(int argc, char **argv)
{
    if (argc != 3)
    {
        fprintf(stderr, "Usage: chgrp <group> <file>\n");
        return EXIT_FAILURE;
    }

    struct group *gr = getgrnam(argv[1]);
    if (gr == NULL)
    {
        fprintf(stderr, "chown: group '%s' not found: %s\n", argv[1],
                strerror(errno));
        return EXIT_FAILURE;
    }
    gid_t group = gr->gr_gid;

    if (chown(argv[2], -1, group) < 0)
    {
        fprintf(stderr, "chgrp failed: %s\n", strerror(errno));
        return EXIT_FAILURE;
    }
    return 0;
}
