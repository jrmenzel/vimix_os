/* SPDX-License-Identifier: MIT */

// Mutual exclusion spin locks.

#include <kernel/cpu.h>
#include <kernel/kernel.h>
#include <kernel/proc.h>
#include <kernel/smp.h>
#include <kernel/spinlock.h>

#ifdef CONFIG_DEBUG_SPINLOCK
#include <kernel/string.h>
#endif

/// initializes a spinlock
void spin_lock_init(struct spinlock *lk, char *name_for_debug)
{
    atomic_init(&lk->locked, false);
#ifdef CONFIG_DEBUG_SPINLOCK
    lk->name = name_for_debug;
    lk->cpu = NULL;
#endif  // CONFIG_DEBUG_SPINLOCK
}

void debug_test_lock(struct spinlock *lk)
{
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

void spin_lock(struct spinlock *lk)
{
    cpu_push_disable_device_interrupt_stack();  // disable interrupts to avoid
                                                // deadlock.
    DEBUG_ASSERT_CPU_DOES_NOT_HOLD_LOCK(lk);

    while (atomic_exchange_explicit(&lk->locked, true, memory_order_acquire) !=
           false)
    {
        // while the lock is held by someone else, keep trying
        // -> this is where the name comes from
    }

    debug_test_lock(lk);
}

bool spin_trylock(struct spinlock *lk)
{
    cpu_push_disable_device_interrupt_stack();  // disable interrupts to avoid
                                                // deadlock.
    DEBUG_ASSERT_CPU_DOES_NOT_HOLD_LOCK(lk);

    // if already locked cleanup and return false
    if (atomic_exchange_explicit(&lk->locked, true, memory_order_acquire) ==
        true)
    {
        cpu_pop_disable_device_interrupt_stack();
        return false;
    }

    debug_test_lock(lk);
    return true;
}

void spin_unlock(struct spinlock *lk)
{
#ifdef CONFIG_DEBUG_SPINLOCK
    if (!spin_lock_is_held_by_this_cpu(lk))
    {
        panic("released spinlock without holding it");
    }
    lk->cpu = NULL;
#endif  // CONFIG_DEBUG_SPINLOCK

    // Release the lock, equivalent to lk->locked = false.
    // All memory writes before the unlock are visible to other
    // CPUs that acquire this lock afterwards.
    atomic_store_explicit(&lk->locked, false, memory_order_release);

    cpu_pop_disable_device_interrupt_stack();
}

#ifdef CONFIG_DEBUG_SPINLOCK
/// Check whether this cpu is holding the lock.
/// Interrupts must be off.
bool spin_lock_is_held_by_this_cpu(struct spinlock *lk)
{
    return (atomic_load(&lk->locked) && lk->cpu == get_cpu());
}
#endif  // CONFIG_DEBUG_SPINLOCK
