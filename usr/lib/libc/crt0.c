/* SPDX-License-Identifier: MIT */

#include <stdint.h>
#include <unistd.h>

#include "stdio_impl.h"
#include "stdlib_impl.h"

// main from the app
extern int32_t main(int argc, char *argv[]);
extern void (*at_exit_function[ATEXIT_MAX])(void);

/// A wrapper for main() to init the std lib and
/// so that main() can return instead of having to call exit().
void _start(int argc, char *argv[])
{
    init_stdio();

    int32_t ret = main(argc, argv);
    exit(ret);
}

/// exit() is a small wrapper around the syscall to call functions registered
/// with atexit()
extern void _sys_exit(int32_t status) __attribute__((noreturn));
void exit(int32_t status)
{
    for (ssize_t i = ATEXIT_MAX - 1; i >= 0; --i)
    {
        if (at_exit_function[i])
        {
            at_exit_function[i]();
        }
    }

    _sys_exit(status);
}
