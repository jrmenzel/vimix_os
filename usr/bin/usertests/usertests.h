/* SPDX-License-Identifier: MIT */

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
extern struct test quicktests_host[];
extern struct test slowtests_host[];

// get free memory to check for leaks
int countfree();
