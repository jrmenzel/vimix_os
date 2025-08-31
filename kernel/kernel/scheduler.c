/* SPDX-License-Identifier: MIT */

#include <kernel/cpu.h>
#include <kernel/kernel.h>
#include <kernel/proc.h>
#include <kernel/process.h>
#include <kernel/scheduler.h>
#include <kernel/spinlock.h>

/// @brief Gets the next runnable process, locked.
/// @return Locked process or NULL.
struct process *get_next_runnable_process()
{
    struct list_head *pos;
    spin_lock(&g_process_list.lock);
    list_for_each(pos, &g_process_list.plist)
    {
        struct process *proc = process_from_list(pos);
        if (spin_trylock(&proc->lock))
        {
            if (proc->state == RUNNABLE)
            {
                // move process to end of list so next scheduling will pick
                // another process (if runnable)
                list_del(pos);
                list_add_tail(pos, &g_process_list.plist);

                spin_unlock(&g_process_list.lock);
                return proc;  // return locked process
            }
            spin_unlock(&proc->lock);
        }
    }
    spin_unlock(&g_process_list.lock);
    return NULL;
}

void scheduler()
{
    struct cpu *cpu = get_cpu();
    cpu->proc = NULL;

    while (true)
    {
        // Avoid deadlock by ensuring that devices can interrupt.
        cpu_enable_interrupts();
        if (cpu->state == CPU_PANICKED) goto KERNEL_PANIC;

        if (cpu->state == CPU_STARTED)
        {
            struct process *proc = get_next_runnable_process();
            if (proc != NULL)
            {
                // printk("run %s\n", proc->name);
                struct cpu *this_cpu = get_cpu();

                if (proc->trapframe == NULL) panic("NULL");
                if (proc->context.ra == 0) panic("0");

                proc_shrink_stack(proc);

                // Switch to chosen process.  It is the process's job
                // to release its lock and then reacquire it
                // before jumping back to us.
                proc->state = RUNNING;
                this_cpu->proc = proc;
                context_switch(&this_cpu->context, &proc->context);

                // Process is done running for now.
                // It should have changed its proc->state before coming
                // back.
                this_cpu->proc = NULL;
                spin_unlock(&proc->lock);
            }
            else
            {
                cpu_enable_interrupts();
                wait_for_interrupt();
            }
        }
    }

KERNEL_PANIC:
    while (true)
    {
        cpu_enable_interrupts();
        wait_for_interrupt();
    }
}
