/* SPDX-License-Identifier: MIT */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/reboot.h>

int main(int argc, char **argv)
{
    if (argc == 1)
    {
        reboot(VIMIX_REBOOT_CMD_POWER_OFF);
        return -2;
    }

    if (argc == 2)
    {
        if ((argv[1][0] == '-') && (argv[1][2] == 0))
        {
            if (argv[1][1] == 'h')
            {
                reboot(VIMIX_REBOOT_CMD_POWER_OFF);
                return -2;
            }
            else if (argv[1][1] == 'r')
            {
                reboot(VIMIX_REBOOT_CMD_RESTART);
                return -2;
            }
        }
    }

    fprintf(stderr, "usage: shutdown [options]\n");
    fprintf(stderr, "options:\n");
    fprintf(stderr, "  -h halt\n");
    fprintf(stderr, "  -r reboot\n");

    return -1;
}