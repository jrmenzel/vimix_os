/* SPDX-License-Identifier: MIT */

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int main(int argc, char *argv[])
{
    if (argc != 3)
    {
        fprintf(stderr, "Usage: ln target link-name\n");
        return 1;
    }
    if (link(argv[1], argv[2]) < 0)
    {
        fprintf(stderr, "link %s %s: failed, errno: %s\n", argv[1], argv[2],
                strerror(errno));
        return 1;
    }
    return 0;
}
