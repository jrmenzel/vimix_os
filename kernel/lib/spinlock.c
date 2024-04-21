/* SPDX-License-Identifier: MIT */

// Mutual exclusion spin locks.

#include <kernel/cpu.h>
#include <kernel/param.h>
#include <kernel/printk.h>
#include <kernel/proc.h>
#include <kernel/spinlock.h>
#include <kernel/types.h>
#include <mm/memlayout.h>

void spin_lock_init(struct spinlock *lk, char *name)
{
    lk->name = name;
    lk->locked = 0;
    lk->cpu = 0;
}

/// Acquire the lock.
/// Loops (spins) until the lock is acquired.
void spin_lock(struct spinlock *lk)
{
    cpu_push_disable_device_interrupt_stack();  // disable interrupts to avoid
                                                // deadlock.
    if (spin_lock_is_held_by_this_cpu(lk)) panic("acquire");

    // On RISC-V, sync_lock_test_and_set turns into an atomic swap:
    //   a5 = 1
    //   s1 = &lk->locked
    //   amoswap.w.aq a5, a5, (s1)
    while (__sync_lock_test_and_set(&lk->locked, 1) != 0)
        ;

    // Tell the C compiler and the processor to not move loads or stores
    // past this point, to ensure that the critical section's memory
    // references happen strictly after the lock is acquired.
    // On RISC-V, this emits a fence instruction.
    __sync_synchronize();

    // Record info about lock acquisition for holding() and debugging.
    lk->cpu = get_cpu();
}

/// Release the lock.
void spin_unlock(struct spinlock *lk)
{
    if (!spin_lock_is_held_by_this_cpu(lk)) panic("release");

    lk->cpu = 0;

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

/// Check whether this cpu is holding the lock.
/// Interrupts must be off.
int spin_lock_is_held_by_this_cpu(struct spinlock *lk)
{
    int r;
    r = (lk->locked && lk->cpu == get_cpu());
    return r;
}
