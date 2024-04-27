/* SPDX-License-Identifier: MIT */

// Create a zombie process that
// must be reparented at exit.

#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>

int main()
{
    if (fork() > 0)
    {
        sleep(5);  // Let child exit before parent.
    }

    return 0;
}
