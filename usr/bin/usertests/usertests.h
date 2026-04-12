/* SPDX-License-Identifier: MIT */

#include <errno.h>
#include <fcntl.h>
#include <kernel/vimixfs.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/signal.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
#include <vimixutils/libasm.h>

#if defined(BUILD_ON_HOST)
#include <linux/limits.h>
#else
#include <kernel/limits.h>
#endif

#define BUFSZ (32 * BLOCK_SIZE)
extern char buf[BUFSZ];

typedef uint32_t test_mask_t;

enum test_mask
{
    TEST_MASK_NONE = 1 << 0,
    TEST_MASK_MEMORY_SIZE = 1 << 1,
    TEST_MASK_CORE_COUNT = 1 << 2,
    TEST_MASK_FS_SIZE = 1 << 3,
    TEST_MASK_FILESYSTEM = 1 << 4,
    TEST_MASK_BITWIDTH = 1 << 5,
};

#define ALL_TESTS_MASK (-1)

struct test
{
    void (*f)(char *);
    char *s;
    test_mask_t mask;
};

extern struct test tests_vimix[];

// host tests run on the host and VIMIX
extern struct test tests_common[];

// used on VIMIX to make memory usage predictable
void prepare_test_environment();

void reset_test_environment();

// get free memory to check for leaks
size_t memory_allocated();

// all helpers below call exit(1) on failure of testing

// test if f is not NULL
void assert_open_ok(const char *test_name, FILE *f, const char *file_name);

// test if fd is not -1
void assert_open_ok_fd(const char *test_name, int fd, const char *file_name);

// test if a signed value is 0
void assert_null_s(const char *test_name, ssize_t value);

// write the NULL-terminated string to the file, but without the 0-terminator!
void assert_write_to_file(const char *test_name, int fd, const char *string);

#define assert_same_value(val_a, val_b)                              \
    if (val_a != val_b)                                              \
    {                                                                \
        printf("%s: error: values mismatch in %s:%d\n", s, __FILE__, \
               __LINE__);                                            \
        exit(1);                                                     \
    }

#define assert_same_string(val_a, val_b)                                    \
    if (strcmp(val_a, val_b) != 0)                                          \
    {                                                                       \
        printf("%s: error: strings mismatch, is:\n%s\nshould be:\n%s\n", s, \
               val_a, val_b);                                               \
        exit(1);                                                            \
    }

#define assert_errno(value)                                                   \
    if (errno != value)                                                       \
    {                                                                         \
        printf(                                                               \
            "%s: error: errno value mismatch! (is: '%s', should be: '%s')\n", \
            s, strerror(errno), strerror(value));                             \
        exit(1);                                                              \
    }

#define assert_error(value)                                              \
    if (value >= 0)                                                      \
    {                                                                    \
        printf("%s: error: call succeeded but should have failed\n", s); \
        exit(1);                                                         \
    }

static inline void assert_no_error_func(int value, const char *s)
{
    if (value < 0)
    {
        printf("%s: error: %d returned (errno: %s)\n", s, (int)value,
               strerror(errno));
        exit(1);
    }
}

#define assert_no_error(value) assert_no_error_func(value, s)

#define assert_no_ptr_error(ptr)                                              \
    if (ptr == NULL)                                                          \
    {                                                                         \
        printf("%s: error: NULL returned (errno: %s)\n", s, strerror(errno)); \
        exit(1);                                                              \
    }
