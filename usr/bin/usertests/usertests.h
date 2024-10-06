/* SPDX-License-Identifier: MIT */

#include <errno.h>
#include <fcntl.h>
#include <kernel/xv6fs.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/signal.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/wait.h>
#include <unistd.h>
#include "../libasm.h"

#if defined(BUILD_ON_HOST)
#include <linux/limits.h>
#else
#include <kernel/limits.h>
#endif

#define BUFSZ ((MAX_OP_BLOCKS + 2) * BLOCK_SIZE)
extern char buf[BUFSZ];

struct test
{
    void (*f)(char *);
    char *s;
};

extern struct test quicktests[];
extern struct test slowtests[];

// host tests run on the host and VIMIX
extern struct test quicktests_common[];
extern struct test slowtests_common[];

// get free memory to check for leaks
int countfree();

// all helpers below call exit(1) on failure of testing

// test if fd is not 0
void assert_open_ok(const char *test_name, int fd, const char *file_name);

// test if a signed value is 0
void assert_null_s(const char *test_name, ssize_t value);

// write the NULL-terminated string to the file, but without the 0-terminator!
void assert_write_to_file(const char *test_name, int fd, const char *string);

#define assert_same_value(val_a, val_b)             \
    if (val_a != val_b)                             \
    {                                               \
        printf("%s: error: values mismatch!\n", s); \
        exit(1);                                    \
    }

#define assert_errno(value)                                                  \
    if (errno != value)                                                      \
    {                                                                        \
        printf("%s: error: errno value mismatch! (is: %d, should be: %d)\n", \
               s, errno, value);                                             \
        exit(1);                                                             \
    }

#define assert_no_error(value)                                    \
    if (value < 0)                                                \
    {                                                             \
        printf("%s: error: -1 returned (errno: %d)\n", s, errno); \
        exit(1);                                                  \
    }
