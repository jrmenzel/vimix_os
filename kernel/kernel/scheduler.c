/* SPDX-License-Identifier: MIT */

#include <kernel/cpu.h>
#include <kernel/kernel.h>
#include <kernel/proc.h>
#include <kernel/spinlock.h>

extern struct process g_process_list[MAX_PROCS];

void scheduler()
{
    struct process *proc;
    struct cpu *cpu = get_cpu();

    cpu->proc = 0;
    for (;;)
    {
        // Avoid deadlock by ensuring that devices can interrupt.
        cpu_enable_device_interrupts();

        for (proc = g_process_list; proc < &g_process_list[MAX_PROCS]; proc++)
        {
            spin_lock(&proc->lock);
            if (proc->state == RUNNABLE)
            {
                // Switch to chosen process.  It is the process's job
                // to release its lock and then reacquire it
                // before jumping back to us.
                proc->state = RUNNING;
                cpu->proc = proc;
                context_switch(&cpu->context, &proc->context);

                // Process is done running for now.
                // It should have changed its proc->state before coming back.
                cpu->proc = 0;
            }
            spin_unlock(&proc->lock);
        }
    }
}
