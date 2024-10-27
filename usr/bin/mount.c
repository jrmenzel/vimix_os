/* SPDX-License-Identifier: MIT */

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/mount.h>

void print_help() { printf("usage: mount -t <type> <source> <mount_point>\n"); }

int main(int argc, char *argv[])
{
    if (argc != 5)
    {
        print_help();
        return -1;
    }
    const char *type = NULL;
    if ((argv[1][0] == '-') && (argv[1][1] == 't') && (argv[1][2] == 0))
    {
        type = argv[2];
    }
    else
    {
        print_help();
        return -1;
    }
    int ret = mount(argv[3], argv[4], type, 0, NULL);
    if (ret != 0)
    {
        printf("mount error: %s\n", strerror(errno));
    }
    return ret;
}
