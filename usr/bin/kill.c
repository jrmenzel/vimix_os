/* SPDX-License-Identifier: MIT */

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
    for (size_t i = 1; i < argc; i++)
    {
        kill(atoi(argv[i]), SIGKILL);
    }

    return 0;
}
