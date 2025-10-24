/* SPDX-License-Identifier: MIT */

#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int main(int argc, char *argv[])
{
    uid_t uid = 0;
    if (argc != 2)
    {
        fprintf(stderr, "Usage: %s <USERID>\n", argv[0]);
        return 1;
    }

    uid = (uid_t)atoi(argv[1]);

    if (setuid(uid) < 0)
    {
        fprintf(stderr, "su: setuid(%d) failed: %s\n", uid, strerror(errno));
        return 1;
    }

    char *shell_argv[] = {"sh", 0};
    if (execv("/usr/bin/sh", shell_argv) < 0)
    {
        fprintf(stderr, "su: execv(/usr/bin/sh) failed: %s\n", strerror(errno));
        return 1;
    }
    return 1;  // should not be reached
}
