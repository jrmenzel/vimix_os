/* SPDX-License-Identifier: MIT */

#include <arch/context.h>
#include <arch/trapframe.h>
#include <kernel/printk.h>

size_t trapframe_get_argument_register(struct trapframe *frame,
                                       size_t register_index)
{
    switch (register_index)
    {
        case 0: return frame->x0;
        case 1: return frame->x1;
        case 2: return frame->x2;
        case 3: return frame->x3;
        case 4: return frame->x4;
        case 5: return frame->x5;
    }

    printk("unexpected register index: %zx\n", register_index);
    panic("trapframe_get_argument_register() called with wrong index");
    return 0xDEAD;
}

void trapframe_set_argument_register(struct trapframe *frame,
                                     size_t register_index, size_t value)
{
    switch (register_index)
    {
        case 0: frame->x0 = value; return;
        case 1: frame->x1 = value; return;
        case 2: frame->x2 = value; return;
        case 3: frame->x3 = value; return;
        case 4: frame->x4 = value; return;
        case 5: frame->x5 = value; return;
        default:
            panic("trapframe_set_argument_register() called with wrong index");
    }
}

void debug_print_process_registers(struct trapframe *frame)
{
    struct trapframe *tf = frame;
    // clang-format off
    printk("x0:  " FORMAT_REG_SIZE "; x1:  " FORMAT_REG_SIZE "; x2:  " FORMAT_REG_SIZE "; x3:  " FORMAT_REG_SIZE "\n", tf->x0,  tf->x1,  tf->x2,  tf->x3);
    printk("x4:  " FORMAT_REG_SIZE "; x5:  " FORMAT_REG_SIZE "; x6:  " FORMAT_REG_SIZE "; x7:  " FORMAT_REG_SIZE "\n", tf->x4,  tf->x5,  tf->x6,  tf->x7);
    printk("x8:  " FORMAT_REG_SIZE "; x9:  " FORMAT_REG_SIZE "; x10: " FORMAT_REG_SIZE "; x11: " FORMAT_REG_SIZE "\n", tf->x8,  tf->x9,  tf->x10, tf->x11);
    printk("x12: " FORMAT_REG_SIZE "; x13: " FORMAT_REG_SIZE "; x14: " FORMAT_REG_SIZE "; x15: " FORMAT_REG_SIZE "\n", tf->x12, tf->x13, tf->x14, tf->x15);
    printk("x16: " FORMAT_REG_SIZE "; x17: " FORMAT_REG_SIZE "; x18: " FORMAT_REG_SIZE "; x19: " FORMAT_REG_SIZE "\n", tf->x16, tf->x17, tf->x18, tf->x19);
    printk("x20: " FORMAT_REG_SIZE "; x21: " FORMAT_REG_SIZE "; x22: " FORMAT_REG_SIZE "; x23: " FORMAT_REG_SIZE "\n", tf->x20, tf->x21, tf->x22, tf->x23);
    printk("x24: " FORMAT_REG_SIZE "; x25: " FORMAT_REG_SIZE "; x26: " FORMAT_REG_SIZE "; x27: " FORMAT_REG_SIZE "\n", tf->x24, tf->x25, tf->x26, tf->x27);
    printk("x28: " FORMAT_REG_SIZE "; x29: " FORMAT_REG_SIZE "; x30: " FORMAT_REG_SIZE "\n",                           tf->x28, tf->x29, tf->x30);
    // clang-format on
}
