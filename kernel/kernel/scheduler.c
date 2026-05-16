/* SPDX-License-Identifier: MIT */

#include <kernel/cpu.h>
#include <kernel/kernel.h>
#include <kernel/kticks.h>
#include <kernel/proc.h>
#include <kernel/process.h>
#include <kernel/scheduler.h>
#include <kernel/spinlock.h>
#include <kernel/stdatomic.h>

/// @brief Gets the next runnable process, locked.
/// @return Locked process or NULL.
struct process *get_next_runnable_process()
{
    struct list_head *pos;
    rwspin_write_lock(&g_process_list.lock);
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

                rwspin_write_unlock(&g_process_list.lock);
                return proc;  // return locked process
            }
            spin_unlock(&proc->lock);
        }
    }
    rwspin_write_unlock(&g_process_list.lock);
    return NULL;
}

atomic_size_t g_last_wakeup_tick = 0;

void wakeup_procs_waiting_on_timer()
{
    size_t now = kticks_get_ticks();
    size_t last_wakeup = atomic_load(&g_last_wakeup_tick);

    if (now == last_wakeup)
    {
        // already woke up procs for this tick, no need to do it again
        return;
    }

    // test atomically: if g_last_wakeup_tick is still last_wakeup, set it to
    // now and retrun true otherwise, another cpu already updated it.
    if (atomic_compare_exchange_weak(&g_last_wakeup_tick, &last_wakeup, now))
    {
        wakeup(&g_ticks);
    }
}

void refresh_kernel_page_table()
{
    size_t epoch = atomic_load(&g_kernel_pagetable->epoch);
    struct cpu *cpu = get_cpu();
    if ((cpu->kernel_pgtable_epoch_seen < epoch) ||
        (atomic_load(&g_kernel_pagetable->update_epoch_pending)))
    {
        spin_lock(&g_kernel_pagetable->lock);
        mmu_set_kernel_page_table(g_kernel_pagetable);
        spin_unlock(&g_kernel_pagetable->lock);
    }
}

void scheduler()
{
    struct cpu *cpu = get_cpu();
    cpu->proc = NULL;
    // A CPU entering scheduler for the first time has no older kernel TLB
    // state to invalidate; baseline to current epoch.
    cpu->kernel_pgtable_epoch_seen = atomic_load(&g_kernel_pagetable->epoch);

    while (true)
    {
        // Avoid deadlock by ensuring that devices can interrupt.
        cpu_enable_interrupts();
        if (cpu->state == CPU_PANICKED) goto KERNEL_PANIC;

        if (cpu->state == CPU_STARTED)
        {
            wakeup_procs_waiting_on_timer();

            struct process *proc = get_next_runnable_process();
            if (proc != NULL)
            {
                struct cpu *this_cpu = get_cpu();

                proc_shrink_stack(proc);

                // The process relies on a correctly mapped kernel stack.
                // make sure the latest kernel page table is loaded.
                refresh_kernel_page_table();

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
