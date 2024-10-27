/* SPDX-License-Identifier: MIT */

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/mount.h>

void print_help() { printf("usage: umount <mount_point>\n"); }

int main(int argc, char *argv[])
{
    if (argc != 2)
    {
        print_help();
        return -1;
    }

    int ret = umount(argv[1]);
    if (ret != 0)
    {
        printf("umount error: %s\n", strerror(errno));
    }
    return ret;
}
