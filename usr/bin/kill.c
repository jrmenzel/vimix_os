/* SPDX-License-Identifier: MIT */

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/signal.h>
#include <unistd.h>

int main(int argc, char **argv)
{
    if (argc < 2)
    {
        fprintf(stderr, "usage: kill pid...\n");
        return 1;
    }

    size_t error_count = 0;
    for (size_t i = 1; i < argc; i++)
    {
        int32_t pid = atoi(argv[i]);
        int ret = kill(pid, SIGKILL);
        if (ret != 0)
        {
            error_count++;
            if (errno == ESRCH)
            {
                printf("No such process with PID %d\n", pid);
            }
        }
    }

    return error_count;
}
