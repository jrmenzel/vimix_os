/* SPDX-License-Identifier: MIT */

/// Test that fork fails gracefully.
/// Tiny executable so that the limit can be filling the g_process_list table.

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

int main()
{
    const int32_t N = 1000;
    pid_t n;

    printf("fork test\n");

    for (n = 0; n < N; n++)
    {
        pid_t pid = fork();
        if (pid < 0) break;
        if (pid == 0) return 0;
    }

    if (n == N)
    {
        printf("fork worked %d times! Expected a failure.\n", N);
        return 1;
    }
    printf("fork worked %d times\n", n);

    for (; n > 0; n--)
    {
        if (wait(NULL) < 0)
        {
            printf("wait stopped early\n");
            return 1;
        }
    }

    if (wait(NULL) != -1)
    {
        printf("wait got too many\n");
        return 1;
    }

    printf("fork test OK\n");
    printf("ALL TESTS PASSED\n");
    return 0;
}
