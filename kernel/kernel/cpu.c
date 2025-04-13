/* SPDX-License-Identifier: MIT */

#include <kernel/cpu.h>
#include <kernel/proc.h>

void cpu_push_disable_device_interrupt_stack()
{
    bool old = cpu_is_interrupts_enabled();

    cpu_disable_interrupts();
    struct cpu *cpu = get_cpu();

    if (cpu->disable_dev_int_stack_depth == 0)
    {
        cpu->disable_dev_int_stack_original_state = old;
    }
    cpu->disable_dev_int_stack_depth += 1;
}

void cpu_pop_disable_device_interrupt_stack()
{
    if (cpu_is_interrupts_enabled())
    {
        panic("cpu_pop_disable_device_interrupt_stack - interruptible");
    }

    struct cpu *cpu = get_cpu();
    if (cpu->disable_dev_int_stack_depth < 1)
    {
        panic("cpu_pop_disable_device_interrupt_stack: stack underrun");
    }

    cpu->disable_dev_int_stack_depth -= 1;
    if (cpu->disable_dev_int_stack_depth == 0 &&
        cpu->disable_dev_int_stack_original_state)
    {
        cpu_enable_interrupts();
    }
}
