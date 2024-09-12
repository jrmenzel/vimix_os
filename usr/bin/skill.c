/* SPDX-License-Identifier: MIT */

// Stack Kill
// Overflow the stack with a recursive function.

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

size_t foo(size_t x)
{
    if (x == 0) return 0;
    size_t y = foo(x - 1);
    return y + 1;
}

int main(int argc, char *argv[])
{
    for (size_t i = 0; i < 1000; ++i)
    {
        printf("Foo of %ld is %ld\n", i, foo(i));
    }

    return 0;
}
