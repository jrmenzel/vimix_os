/* SPDX-License-Identifier: MIT */

// Mutual exclusion spin locks.

#include <kernel/cpu.h>
#include <kernel/kernel.h>
#include <kernel/proc.h>
#include <kernel/rwspinlock.h>
#include <kernel/smp.h>

#ifdef CONFIG_DEBUG_SPINLOCK
#include <kernel/string.h>
#endif

/// initializes a rwspinlock
void rwspin_lock_init(struct rwspinlock *lk, char *name_for_debug)
{
    atomic_init(&lk->locked, false);
    atomic_init(&lk->readers, 0);

#ifdef CONFIG_DEBUG_SPINLOCK
    lk->name = name_for_debug;
    lk->cpu = NULL;
#endif  // CONFIG_DEBUG_SPINLOCK
}

void debug_test_rwlock(struct rwspinlock *lk)
{
#ifdef CONFIG_DEBUG_SPINLOCK
    if (lk->cpu != NULL)
    {
        if (lk->cpu == get_cpu())
        {
            panic("rwspin_lock is owned by this CPU already");
        }
        else
        {
            panic("rwspin_lock was not cleared at release");
        }
    }
    // Record info about lock acquisition for holding() and debugging.
    lk->cpu = get_cpu();
#endif  // CONFIG_DEBUG_SPINLOCK
}

void rwspin_read_lock(struct rwspinlock *lk)
{
    cpu_push_disable_device_interrupt_stack();  // disable interrupts to avoid
                                                // deadlock.
    DEBUG_ASSERT_CPU_DOES_NOT_HOLD_RWLOCK(lk);

    while (atomic_exchange_explicit(&lk->locked, true, memory_order_acquire) !=
           false)
    {
        // while the lock is held by someone else, keep trying
        // -> this is where the name comes from
    }

    atomic_fetch_add_explicit(&lk->readers, 1, memory_order_acquire);

    // unlock:
    atomic_store_explicit(&lk->locked, false, memory_order_release);
}

void rwspin_write_lock(struct rwspinlock *lk)
{
    cpu_push_disable_device_interrupt_stack();  // disable interrupts to avoid
                                                // deadlock.
    DEBUG_ASSERT_CPU_DOES_NOT_HOLD_RWLOCK(lk);

    while (atomic_exchange_explicit(&lk->locked, true, memory_order_acquire) !=
           false)
    {
        // while the lock is held by someone else, keep trying
        // -> this is where the name comes from
    }

    // wait until all readers are done
    while (atomic_load_explicit(&lk->readers, memory_order_acquire) != 0)
    {
        // wait
    }

    debug_test_rwlock(lk);
}

void rwspin_read_unlock(struct rwspinlock *lk)
{
#ifdef CONFIG_DEBUG_SPINLOCK
    lk->cpu = NULL;
#endif  // CONFIG_DEBUG_SPINLOCK

    atomic_fetch_sub_explicit(&lk->readers, 1, memory_order_release);

    cpu_pop_disable_device_interrupt_stack();
}

void rwspin_write_unlock(struct rwspinlock *lk)
{
#ifdef CONFIG_DEBUG_SPINLOCK
    if (!rwspin_lock_is_held_by_this_cpu(lk))
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
bool rwspin_lock_is_held_by_this_cpu(struct rwspinlock *lk)
{
    return (atomic_load(&lk->locked) && lk->cpu == get_cpu());
}
#endif  // CONFIG_DEBUG_SPINLOCK
