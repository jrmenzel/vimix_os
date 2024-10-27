/* SPDX-License-Identifier: MIT */

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>
#include <unistd.h>

int error_out()
{
    fprintf(stderr, "Usage: mknod name type major minor\n");
    fprintf(stderr, "type: b = block device, c = char device\n");
    return -1;
}

int main(int argc, char *argv[])
{
    if (argc != 5) return error_out();

    mode_t mode = 0644;

    // parse type and adjust mode
    if (argv[2][0] == 0 || argv[2][1] != 0) return error_out();
    if (argv[2][0] == 'c')
    {
        mode |= S_IFCHR;
    }
    else if (argv[2][0] == 'b')
    {
        mode |= S_IFBLK;
    }
    else
    {
        return error_out();
    }

    // parse device
    dev_t device = makedev(atoi(argv[3]), atoi(argv[4]));
    int ret = mknod(argv[1], mode, device);
    if (ret < 0)
    {
        printf("mknod failed: %s\n", strerror(errno));
        return -1;
    }

    return 0;
}
