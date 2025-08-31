/* SPDX-License-Identifier: MIT */

#include <kernel/cpu.h>
#include <kernel/kernel.h>
#include <kernel/proc.h>
#include <kernel/process.h>
#include <kernel/scheduler.h>
#include <kernel/spinlock.h>

void scheduler()
{
    struct cpu *cpu = get_cpu();
    cpu->proc = NULL;

    while (true)
    {
        // Avoid deadlock by ensuring that devices can interrupt.
        cpu_enable_interrupts();
        bool found_runnable = false;

        for (struct process *proc = g_process_list;
             proc < &g_process_list[MAX_PROCS]; proc++)
        {
            if (cpu->state != CPU_STARTED) goto KERNEL_PANIC;

            // try to lock and check, but if the process is locked,
            // it's probably running, so skip to the next one
            if (spin_trylock(&proc->lock))
            {
                if (proc->state == RUNNABLE)
                {
                    // printk("run %s\n", proc->name);
                    struct cpu *this_cpu = get_cpu();

                    proc_shrink_stack(proc);

                    // Switch to chosen process.  It is the process's job
                    // to release its lock and then reacquire it
                    // before jumping back to us.
                    found_runnable = true;
                    proc->state = RUNNING;
                    this_cpu->proc = proc;
                    context_switch(&this_cpu->context, &proc->context);

                    // Process is done running for now.
                    // It should have changed its proc->state before coming
                    // back.
                    this_cpu->proc = NULL;
                }
                spin_unlock(&proc->lock);
            }
        }

        if (!found_runnable)
        {
            cpu_enable_interrupts();
            wait_for_interrupt();
        }
    }

KERNEL_PANIC:
    while (true)
    {
        cpu_enable_interrupts();
        wait_for_interrupt();
    }
}
