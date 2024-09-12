/* SPDX-License-Identifier: MIT */

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

void print_help() { printf("Usage: sleep <time in seconds>\n"); }

int main(int argc, char *argv[])
{
    if (argc != 2)
    {
        print_help();
        return -1;
    }

    size_t seconds = atoi(argv[1]);
    sleep(seconds);

    return 0;
}
