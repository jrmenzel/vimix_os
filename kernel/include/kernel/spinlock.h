/* SPDX-License-Identifier: MIT */
#pragma once

#include <kernel/kernel.h>

struct cpu;

/// Mutual exclusion lock.
struct spinlock
{
    // On RISC V the atomic exchange operation works on 32bit, so use
    // (u)int32_t.
    volatile uint32_t locked;  ///< Is the lock held?

#ifdef CONFIG_DEBUG_SPINLOCK
    struct cpu *cpu;  ///< The CPU holding the lock.
    char *name;       ///< For debugging: Name of lock.
#endif                // CONFIG_DEBUG_SPINLOCK
};

/// Init a new spinlock
/// @param lk the lock to init
/// @param name_for_debug only used for debugging
void spin_lock_init(struct spinlock *lk, char *name_for_debug);

/// Acquire/lock the spinlock.
void spin_lock(struct spinlock *lk);

/// Release/unlock the spinlock.
void spin_unlock(struct spinlock *lk);

#ifdef CONFIG_DEBUG_SPINLOCK
/// Check whether this cpu is holding the lock. Interrupts must be off.
bool spin_lock_is_held_by_this_cpu(struct spinlock *lk);

/// verifies that the CPU holds the lock
/// lock is a struct spinlock*
#define DEBUG_ASSERT_CPU_HOLDS_LOCK(lock)                           \
    if (spin_lock_is_held_by_this_cpu(lock) == false)               \
    {                                                               \
        panic("debug assert failed: spin lock is not held by CPU"); \
    }

/// verifies that the CPU does not hold the lock
/// lock is a struct spinlock*
#define DEBUG_ASSERT_CPU_DOES_NOT_HOLD_LOCK(lock)               \
    if (spin_lock_is_held_by_this_cpu(lock) == true)            \
    {                                                           \
        panic("debug assert failed: spin lock is held by CPU"); \
    }

#else

/// no-op
#define DEBUG_ASSERT_CPU_HOLDS_LOCK(lock)

// no-op
#define DEBUG_ASSERT_CPU_DOES_NOT_HOLD_LOCK(lock)

#endif  // CONFIG_DEBUG_SPINLOCK
