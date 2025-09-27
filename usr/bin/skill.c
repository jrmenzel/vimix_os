/* SPDX-License-Identifier: MIT */

// Stack Kill
// Overflow the stack with a recursive function.

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <vimixutils/libasm.h>

size_t min_stack = 0;

size_t foo(size_t x)
{
    if (x == 0) return 0;
    size_t y = foo(x - 1);
    size_t sp = asm_read_stack_pointer();
    if (sp < min_stack) min_stack = sp;
    return y + 1;
}

int main(int argc, char *argv[])
{
    size_t loop = 1200;
    if (argc > 1)
    {
        loop = atoi(argv[1]);
    }

    // total stack usage is more, but good enough to see when multiple pages are
    // needed for the stack
    size_t stack_start = asm_read_stack_pointer();
    min_stack = stack_start;
    printf("looping %d times\n", (int)loop);
    for (size_t i = 0; i < loop; ++i)
    {
        min_stack = stack_start;
        printf("Foo of %zd is %zd | ", i, foo(i));
        printf("stack size of last loop: 0x%x\n",
               (int)(stack_start - min_stack));
    }

    return 0;
}
