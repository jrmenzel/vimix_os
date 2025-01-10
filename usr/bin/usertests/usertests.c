/* SPDX-License-Identifier: MIT */

#include "usertests.h"

//
// drive tests
//

// run each test in its own process. run returns 1 if child's exit()
// indicates success.
int run(void f(char *), char *s)
{
    time_t start_time = time(NULL);
    printf("test %s: ", s);

    // need a flush, because if the print from above is in the processes "todo
    // list" to print, it will be on the childs todo list as well after fork and
    // both might print the same message
    fflush(stdout);

    pid_t pid = fork();
    if (pid < 0)
    {
        printf("runtest: fork error\n");
        exit(1);
    }
    if (pid == 0)
    {
        f(s);
        exit(0);
    }

    int32_t xstatus;
    wait(&xstatus);
    xstatus = WEXITSTATUS(xstatus);
    if (xstatus != 0)
    {
        printf("FAILED\n");
    }
    else
    {
        printf("OK");
        time_t end_time = time(NULL);
        time_t seconds = end_time - start_time;
        printf(" - %zus\n", (size_t)seconds);
    }
    return xstatus == 0;
}

int runtests(struct test *tests, char *justone)
{
    for (struct test *t = tests; t->s != 0; t++)
    {
        if ((justone == NULL) || strcmp(t->s, justone) == 0)
        {
            if (!run(t->f, t->s))
            {
                printf("SOME TESTS FAILED\n");
                return 1;
            }
        }
    }
    return 0;
}

int drivetests(int quick, int continuous, char *justone)
{
    mkdir("utests-tmp", 0755);
    if (chdir("utests-tmp") < 0) return -1;

    do {
        printf("usertests starting\n");
        int free0 = countfree();
        if (runtests(quicktests_common, justone) ||
            runtests(quicktests, justone))
        {
            if (continuous != 2)
            {
                return 1;
            }
        }
        if (!quick)
        {
            if (justone == 0) printf("usertests slow tests starting\n");
            if (runtests(slowtests_common, justone) ||
                runtests(slowtests, justone))
            {
                if (continuous != 2)
                {
                    return 1;
                }
            }
        }
        int free1 = countfree();
        if (free1 < free0)
        {
            printf("FAILED -- lost some free pages %d (out of %d)\n", free1,
                   free0);
            printf("badarg is a candidate for leaked memory\n");
            if (continuous != 2)
            {
                return 1;
            }
        }
    } while (continuous);

    if (chdir("..") < 0) return -1;
    rmdir("utests-tmp");
    return 0;
}

int main(int argc, char *argv[])
{
    time_t start_time = time(NULL);

    int continuous = 0;
    bool quick_tests_only = false;
    char *justone = NULL;

    if (argc == 2 && strcmp(argv[1], "-q") == 0)
    {
        quick_tests_only = true;
    }
    else if (argc >= 2 && strcmp(argv[1], "-c") == 0)
    {
        continuous = 1;
        if (argc == 3)
        {
            justone = argv[2];
        }
    }
    else if (argc >= 2 && strcmp(argv[1], "-C") == 0)
    {
        continuous = 2;
        if (argc == 3)
        {
            justone = argv[2];
        }
    }
    else if (argc == 2 && argv[1][0] != '-')
    {
        justone = argv[1];
    }
    else if (argc > 1)
    {
        printf("Usage: usertests [-c] [-C] [-q] [testname]\n");
        return 1;
    }
    if (drivetests(quick_tests_only, continuous, justone))
    {
        printf("drivetests failed\n");
        return 1;
    }

    printf("ALL TESTS PASSED\n");
    time_t end_time = time(NULL);
    time_t seconds = end_time - start_time;
    time_t minutes = seconds / 60;
    seconds = seconds % 60;
    printf("Elapsed time: %zum %zus\n", (size_t)minutes, (size_t)seconds);

    return 0;
}
