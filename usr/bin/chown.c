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
        fprintf(stderr, "Usage: chown <user>:<group> <file>\n");
        return EXIT_FAILURE;
    }

    gid_t group = -1;
    char *colon_pos = strchr(argv[1], ':');
    if (colon_pos != NULL)
    {
        *colon_pos = '\0';
        struct group *gr = getgrnam(colon_pos + 1);
        if (gr == NULL)
        {
            fprintf(stderr, "chown: group '%s' not found: %s\n", colon_pos + 1,
                    strerror(errno));
            return EXIT_FAILURE;
        }
        group = gr->gr_gid;
    }

    struct group *gr = getgrnam(argv[1]);
    if (gr == NULL)
    {
        fprintf(stderr, "chown: user '%s' not found: %s\n", argv[1],
                strerror(errno));
        return EXIT_FAILURE;
    }
    gid_t user = gr->gr_gid;

    if (chown(argv[2], user, group) < 0)
    {
        fprintf(stderr, "chown failed: %s\n", strerror(errno));
        return EXIT_FAILURE;
    }
    return 0;
}
