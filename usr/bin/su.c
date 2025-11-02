/* SPDX-License-Identifier: MIT */

#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <vimixutils/security.h>

int main(int argc, char *argv[])
{
    char *user = "root";
    if (argc == 2)
    {
        user = argv[1];
    }
    if (argc > 2)
    {
        fprintf(stderr, "Usage: %s <USER>\n", argv[0]);
        return 1;
    }

    if (!impersonate_user(user, true, false, true))
    {
        fprintf(stderr, "login: failed to impersonate user '%s'\n", user);
        return 1;
    }

    return 1;  // should not be reached
}
