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
