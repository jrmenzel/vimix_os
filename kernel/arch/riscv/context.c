/* SPDX-License-Identifier: MIT */
#include <arch/context.h>
#include <arch/trapframe.h>
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

void debug_print_process_registers(struct trapframe *frame)
{
    struct trapframe *tf = frame;
    // clang-format off
    printk("ra:  " FORMAT_REG_SIZE "; s0: " FORMAT_REG_SIZE "; a0: " FORMAT_REG_SIZE "; t0: " FORMAT_REG_SIZE "\n", tf->ra,  tf->s0, tf->a0, tf->t0);
    printk("sp:  " FORMAT_REG_SIZE "; s1: " FORMAT_REG_SIZE "; a1: " FORMAT_REG_SIZE "; t1: " FORMAT_REG_SIZE "\n", tf->sp,  tf->s1, tf->a1, tf->t1);
    printk("gp:  " FORMAT_REG_SIZE "; s2: " FORMAT_REG_SIZE "; a2: " FORMAT_REG_SIZE "; t2: " FORMAT_REG_SIZE "\n", tf->gp,  tf->s2, tf->a2, tf->t2);
    printk("tp:  " FORMAT_REG_SIZE "; s3: " FORMAT_REG_SIZE "; a3: " FORMAT_REG_SIZE "; t3: " FORMAT_REG_SIZE "\n", tf->tp,  tf->s3, tf->a3, tf->t3);
    printk("s8:  " FORMAT_REG_SIZE "; s4: " FORMAT_REG_SIZE "; a4: " FORMAT_REG_SIZE "; t4: " FORMAT_REG_SIZE "\n", tf->s8,  tf->s4, tf->a4, tf->t4);
    printk("s9:  " FORMAT_REG_SIZE "; s5: " FORMAT_REG_SIZE "; a5: " FORMAT_REG_SIZE "; t5: " FORMAT_REG_SIZE "\n", tf->s9,  tf->s5, tf->a5, tf->t5);
    printk("s10: " FORMAT_REG_SIZE "; s6: " FORMAT_REG_SIZE "; a6: " FORMAT_REG_SIZE "; t6: " FORMAT_REG_SIZE "\n", tf->s10, tf->s6, tf->a6, tf->t6);
    printk("s11: " FORMAT_REG_SIZE "; s7: " FORMAT_REG_SIZE "; a7: " FORMAT_REG_SIZE "\n",               tf->s11, tf->s7, tf->a7);
    // clang-format on
}
