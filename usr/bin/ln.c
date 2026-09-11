/* SPDX-License-Identifier: MIT */

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int main(int argc, char *argv[])
{
    if (argc == 3)
    {
        if (link(argv[1], argv[2]) < 0)
        {
            fprintf(stderr, "link %s %s: failed, errno: %s\n", argv[1], argv[2],
                    strerror(errno));
            return 1;
        }
    }
    else if ((argc == 4) && (argv[1][0] == '-') && (argv[1][1] == 's') &&
             (argv[1][2] == 0))
    {
        if (symlink(argv[2], argv[3]) < 0)
        {
            fprintf(stderr, "symlink %s %s: failed, errno: %s\n", argv[2],
                    argv[3], strerror(errno));
            return 1;
        }
    }
    else
    {
        fprintf(stderr, "Usage for hardlinks: ln target link-name\n");
        fprintf(stderr, "Usage for symlinks: ln -s target link-name\n");
        return 1;
    }
    return 0;
}
