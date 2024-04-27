/* SPDX-License-Identifier: MIT */

// Mutual exclusion spin locks.

#include <kernel/cpu.h>
#include <kernel/kernel.h>
#include <kernel/proc.h>
#include <kernel/spinlock.h>
#include <mm/memlayout.h>

#ifdef CONFIG_DEBUG_SPINLOCK
#include <kernel/string.h>
#endif

/// initializes a spinlock
void spin_lock_init(struct spinlock *lk, char *name_for_debug)
{
    lk->locked = false;
#ifdef CONFIG_DEBUG_SPINLOCK
    lk->name = name_for_debug;
    lk->cpu = NULL;
#endif  // CONFIG_DEBUG_SPINLOCK
}

/// Acquire/lock the lock.
/// Loops (spins) until the lock is acquired.
/// Disables interrupts until the lock gets released.
void spin_lock(struct spinlock *lk)
{
    cpu_push_disable_device_interrupt_stack();  // disable interrupts to avoid
                                                // deadlock.
    DEBUG_ASSERT_CPU_DOES_NOT_HOLD_LOCK(lk);

    // sync_lock_test_and_set() is a GCC build-in function
    // On RISC-V, sync_lock_test_and_set turns into an atomic swap:
    //   a5 = 1
    //   s1 = &lk->locked
    //   amoswap.w.aq a5, a5, (s1)
    while (__sync_lock_test_and_set(&lk->locked, true) != false)
    {
        // while the lock is held by someone else, keep trying
        // -> this is where the name comes from
    }

    // Tell the C compiler and the processor to not move loads or stores
    // past this point, to ensure that the critical section's memory
    // references happen strictly after the lock is acquired.
    // On RISC-V, this emits a fence instruction.
    __sync_synchronize();

#ifdef CONFIG_DEBUG_SPINLOCK
    if (lk->cpu != NULL)
    {
        if (lk->cpu == get_cpu())
        {
            panic("spin_lock is owned by this CPU already");
        }
        else
        {
            panic("spin_lock was not cleared at release");
        }
    }
    // Record info about lock acquisition for holding() and debugging.
    lk->cpu = get_cpu();
#endif  // CONFIG_DEBUG_SPINLOCK
}

/// Release/unlock the lock.
/// Re-enables interrupts (if they were enabled at time of spin_lock())
void spin_unlock(struct spinlock *lk)
{
#ifdef CONFIG_DEBUG_SPINLOCK
    if (!spin_lock_is_held_by_this_cpu(lk))
    {
        panic("released spinlock without holding it");
    }
    lk->cpu = NULL;
#endif  // CONFIG_DEBUG_SPINLOCK

    // Tell the C compiler and the CPU to not move loads or stores
    // past this point, to ensure that all the stores in the critical
    // section are visible to other CPUs before the lock is released,
    // and that loads in the critical section occur strictly before
    // the lock is released.
    // On RISC-V, this emits a fence instruction.
    __sync_synchronize();

    // Release the lock, equivalent to lk->locked = 0.
    // This code doesn't use a C assignment, since the C standard
    // implies that an assignment might be implemented with
    // multiple store instructions.
    // On RISC-V, sync_lock_release turns into an atomic swap:
    //   s1 = &lk->locked
    //   amoswap.w zero, zero, (s1)
    __sync_lock_release(&lk->locked);

    cpu_pop_disable_device_interrupt_stack();
}

#ifdef CONFIG_DEBUG_SPINLOCK
/// Check whether this cpu is holding the lock.
/// Interrupts must be off.
bool spin_lock_is_held_by_this_cpu(struct spinlock *lk)
{
    return (lk->locked && lk->cpu == get_cpu());
}
#endif  // CONFIG_DEBUG_SPINLOCK
