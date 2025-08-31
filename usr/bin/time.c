/* SPDX-License-Identifier: MIT */

#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
#include <vimixutils/path.h>

int main(int argc, char *argv[])
{
    if (argc <= 1)
    {
        fprintf(stderr, "Usage: time command [args...]\n");
        return 1;
    }

    time_t start = time(NULL);

    pid_t pid = fork();
    if (pid < 0)
    {
        fprintf(stderr, "fork failed with error %s\n", strerror(errno));
        exit(1);
    }

    if (pid == 0)
    {
        char *binary_path = find_program_in_path(argv[1]);
        if (binary_path != NULL)
        {
            execv(binary_path, &argv[1]);
            fprintf(stderr, "execv failed with error %s\n", strerror(errno));
            free(binary_path);
        }
        fprintf(stderr, "command not found: %s\n", argv[1]);
        exit(1);
    }

    int32_t xstatus;
    wait(&xstatus);
    time_t end = time(NULL);

    long int delta = (int32_t)(end - start);
    printf("\n");
    printf("real %ldm%lds\n", delta / 60, delta % 60);

    return 0;
}
