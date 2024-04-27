/* SPDX-License-Identifier: MIT */

#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
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
        fprintf(stderr, "link %s %s: failed\n", argv[1], argv[2]);
        return 1;
    }
    return 0;
}
