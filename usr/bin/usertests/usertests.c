/* SPDX-License-Identifier: MIT */

#include "usertests.h"
#include <vimixutils/sysfs.h>
#include <vimixutils/time.h>

//
// drive tests
//

// run each test in its own process. run returns 1 if child's exit()
// indicates success.
bool run(void f(char *), char *s)
{
    uint64_t start_time = get_time_ms();
    size_t alloc_start = memory_allocated();
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
        exit(EXIT_SUCCESS);
    }

    int32_t xstatus;
    wait(&xstatus);
    xstatus = WEXITSTATUS(xstatus);

    size_t alloc_end = memory_allocated();
    if (xstatus != 0)
    {
        printf("FAILED\n");
        if (alloc_start != alloc_end)
        {
            printf(
                " + leaked memory; used at start: %zu, used at end: "
                "%zu\n",
                alloc_start, alloc_end);
        }
    }
    else
    {
        if (alloc_start != alloc_end)
        {
            printf(
                "FAILED due to leaked memory; used at start: %zu, used at end: "
                "%zu\n",
                alloc_start, alloc_end);
            xstatus = -1;  // fail the test
        }
        else
        {
            printf("OK");
        }
        uint64_t end_time = get_time_ms();
        time_t mseconds = end_time - start_time;
        printf(" - %llu.%03llus\n", mseconds / 1000ULL, mseconds % 1000ULL);
    }
    return xstatus == 0;
}

int runtests(struct test *tests, char *justone, test_mask_t mask)
{
    for (struct test *t = tests; t->s != 0; t++)
    {
        if ((justone == NULL) || strcmp(t->s, justone) == 0)
        {
            if ((t->mask & mask) == 0)
            {
                continue;
            }
            if (!run(t->f, t->s))
            {
                printf("SOME TESTS FAILED\n");
                return 1;
            }
        }
    }
    return 0;
}

int drivetests(test_mask_t mask, int continuous, char *justone)
{
    mkdir("/tmp/utests", 0755);
    if (chdir("/tmp/utests") < 0) return -1;

    do
    {
        printf("usertests starting\n");
        size_t pages_free_start = memory_allocated();
        if (runtests(tests_common, justone, mask) ||
            runtests(tests_vimix, justone, mask))
        {
            if (continuous != 2)
            {
                return 1;
            }
        }

        size_t pages_free_end = memory_allocated();
        if (pages_free_start != pages_free_end)
        {
            printf(
                "FAILED -- lost some free pages; free now: %zd (was: %zd); %zd "
                "lost\n",
                pages_free_end, pages_free_start,
                pages_free_start - pages_free_end);
            if (continuous != 2)
            {
                return 1;
            }
        }
    } while (continuous);

    const char *s = "drivetests";
    unlink("x");  // leftover
    assert_no_error(chdir(".."));
    assert_no_error(rmdir("/tmp/utests"));
    return 0;
}

void print_usage()
{
    printf("Usage: usertests [-c] [-C] [testname]\n");
    printf("  -c: run continuously\n");
    printf("  -C: run continuously\n");
    printf("  -m <mask>: run only tests matching the given mask\n");
    printf("  testname: run only the test with the given name\n");
}

int main(int argc, char *argv[])
{
    prepare_test_environment();

    time_t start_time = time(NULL);

    int continuous = 0;
    char *justone = NULL;

    int arg_pos = 1;
    test_mask_t mask = ALL_TESTS_MASK;
    while (true)
    {
        if (arg_pos >= argc)
        {
            break;
        }
        if (strcmp(argv[arg_pos], "-c") == 0)
        {
            continuous = 1;
            arg_pos++;
        }
        else if (strcmp(argv[arg_pos], "-C") == 0)
        {
            continuous = 2;
            arg_pos++;
        }
        else if (strcmp(argv[arg_pos], "-m") == 0)
        {
            arg_pos++;
            if (arg_pos < argc)
            {
                mask = atoi(argv[arg_pos]);
                arg_pos++;
            }
            else
            {
                print_usage();
                return 1;
            }
        }
        else
        {
            justone = argv[arg_pos];
            break;
        }
    }

    if (drivetests(mask, continuous, justone))
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
