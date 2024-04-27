/* SPDX-License-Identifier: MIT */

#include <stdint.h>
#include <unistd.h>

#include "stdio_impl.h"

// main from the app
extern int32_t main();

/// A wrapper for main() to init the std lib and
/// so that main() can return instead of having to call exit().
void _start()
{
    init_stdio();

    int32_t ret = main();
    exit(ret);
}
