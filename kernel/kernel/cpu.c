/* SPDX-License-Identifier: MIT */
/*
 * Based on xv6.
 */

#include <kernel/cpu.h>
#include <kernel/printk.h>
#include <kernel/proc.h>

void cpu_push_disable_device_interrupt_stack()
{
    int old = cpu_is_device_interrupts_enabled();

    cpu_disable_device_interrupts();
    if (get_cpu()->disable_dev_int_stack_depth == 0)
        get_cpu()->disable_dev_int_stack_original_state = old;
    get_cpu()->disable_dev_int_stack_depth += 1;
}

void cpu_pop_disable_device_interrupt_stack()
{
    struct cpu *c = get_cpu();
    if (cpu_is_device_interrupts_enabled())
        panic("cpu_pop_disable_device_interrupt_stack - interruptible");
    if (c->disable_dev_int_stack_depth < 1)
        panic("cpu_pop_disable_device_interrupt_stack");
    c->disable_dev_int_stack_depth -= 1;
    if (c->disable_dev_int_stack_depth == 0 &&
        c->disable_dev_int_stack_original_state)
        cpu_enable_device_interrupts();
}
