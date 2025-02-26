/* SPDX-License-Identifier: MIT */
#include <arch/context.h>
#include <kernel/printk.h>

/// register_index 0 = register a0 (to 5 = a5)
size_t trapframe_get_argument_register(struct trapframe *frame,
                                       size_t register_index)
{
    switch (register_index)
    {
        case 0: return frame->a0;
        case 1: return frame->a1;
        case 2: return frame->a2;
        case 3: return frame->a3;
        case 4: return frame->a4;
        case 5: return frame->a5;
    }

    printk("unexpected register index: %zx\n", register_index);
    panic("trapframe_get_argument_register() called with wrong value");
    return 0xDEAD;
}

void trapframe_set_argument_register(struct trapframe *frame,
                                     size_t register_index, size_t value)
{
    switch (register_index)
    {
        case 0: frame->a0 = value; return;
        case 1: frame->a1 = value; return;
        case 2: frame->a2 = value; return;
        case 3: frame->a3 = value; return;
        case 4: frame->a4 = value; return;
        case 5: frame->a5 = value; return;
        default:
            panic("trapframe_set_argument_register() called with wrong index");
    }
}
