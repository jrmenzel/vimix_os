/* SPDX-License-Identifier: MIT */

#include "usertests.h"

#if defined(BUILD_ON_HOST)
// we are not the only process on the host, so comparing memory isn't useful
int countfree() { return 0; }

struct test quicktests[] = {
    {0, 0},
};

struct test slowtests[] = {
    {0, 0},
};
#endif

char buf[BUFSZ];

/// @brief Reads from /dev/null should return 0, writes to it should return the
/// length of the written string.
/// @param s test name
void dev_null(char *s)
{
    const size_t N = 3;
    int fd = open("/dev/null", O_RDWR);
    if (fd < 0)
    {
        printf("%s: error: could not open /dev/null\n", s);
        exit(1);
    }
    for (size_t i = 0; i < N; i++)
    {
        size_t len = 1 + i;
        ssize_t r = write(fd, "aaaaaaaaaa", len);
        if (r != len)
        {
            printf("%s: error: write to /dev/null failed\n", s);
            exit(1);
        }
        r = read(fd, buf, len);
        if (r != 0)
        {
            printf("%s: read of /dev/null should return 0\n", s);
            exit(1);
        }
    }
    close(fd);
}

/// @brief Reads from /dev/zero should fill the buffer with '0', writes to it
/// should return the length of the written string.
/// @param s test name
void dev_zero(char *s)
{
    const size_t N = 4;
    int fd = open("/dev/zero", O_RDWR);
    if (fd < 0)
    {
        printf("%s: error: could not open /dev/zero\n", s);
        exit(1);
    }
    for (size_t i = 0; i < N; i++)
    {
        size_t len = 1 + i;
        ssize_t r = write(fd, "aaaaaaaaaa", len);
        if (r != len)
        {
            printf("%s: error: write to /dev/zero failed\n", s);
            exit(1);
        }

        if (i == N - 1)
        {
            len = 5000;  // > 1 PAGE_SIZE
        }
        for (size_t j = 0; j < len; ++j)
        {
            buf[j] = 0xFF;
        }
        r = read(fd, buf, len);
        if (r != len)
        {
            printf("%s: read of /dev/zero failed\n", s);
            exit(1);
        }
        for (size_t j = 0; j < len; ++j)
        {
            if (buf[j] != 0)
            {
                printf("%s: read of /dev/zero did not return 0 at pos %zd\n", s,
                       j);
                exit(1);
            }
        }
    }
    close(fd);
}

struct test quicktests_host[] = {
    {dev_null, "dev_null"},
    {dev_zero, "dev_zero"},

    {0, 0},
};

struct test slowtests_host[] = {
    {0, 0},
};
