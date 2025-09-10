/* SPDX-License-Identifier: MIT */
#pragma once

#include <kernel/kernel.h>
#include <kernel/stdatomic.h>

struct cpu;

/// Lock which allows multiple readers or one writer.
/// Writer preference: If a writer is waiting, no new readers are allowed to
/// acquire the lock. This prevents writer starvation, but readers may starve if
/// there is a constant stream of writers.
/// Can be used to guard a list which is mostly read and rarely changed.
struct rwspinlock
{
    atomic_bool locked;     ///< Is the lock held?
    atomic_size_t readers;  ///< Number of readers holding the lock.

#ifdef CONFIG_DEBUG_SPINLOCK
    struct cpu *cpu;  ///< The CPU holding the lock.
    char *name;       ///< For debugging: Name of lock.
#endif                // CONFIG_DEBUG_SPINLOCK
};

/// Init a new rwspinlock
/// @param lk the lock to init
/// @param name_for_debug only used for debugging
void rwspin_lock_init(struct rwspinlock *lk, char *name_for_debug);

/// @brief Acquire/lock the lock for a reader. When aquired, other readers can
/// also hold the lock but no writers.
/// Loops (spins) until the lock is acquired. Disables
/// interrupts until the lock gets released.
/// @param lk The rwspinlock.
void rwspin_read_lock(struct rwspinlock *lk);

/// @brief Acquire/lock the lock for a writer. When aquired, no other readers or
/// writers can hold the lock.
/// Loops (spins) until the lock is acquired.
/// Disables interrupts until the lock gets released.
/// @param lk The rwspinlock.
void rwspin_write_lock(struct rwspinlock *lk);

/// @brief Release/unlock the lock from a reader.
/// Re-enables interrupts (if they were enabled at time of spin_lock())
/// @param lk The rwspinlock.
void rwspin_read_unlock(struct rwspinlock *lk);

/// @brief Release/unlock the lock from a writer.
/// Re-enables interrupts (if they were enabled at time of spin_lock())
/// @param lk The rwspinlock.
void rwspin_write_unlock(struct rwspinlock *lk);

#ifdef CONFIG_DEBUG_SPINLOCK
/// Check whether this cpu is holding the lock. Interrupts must be off.
bool rwspin_lock_is_held_by_this_cpu(struct rwspinlock *lk);

/// verifies that the CPU holds the lock
/// lock is a struct rwspinlock*
#define DEBUG_ASSERT_CPU_HOLDS_RWLOCK(lock)                         \
    if (rwspin_lock_is_held_by_this_cpu(lock) == false)             \
    {                                                               \
        panic("debug assert failed: spin lock is not held by CPU"); \
    }

/// verifies that the CPU does not hold the lock
/// lock is a struct rwspinlock*
#define DEBUG_ASSERT_CPU_DOES_NOT_HOLD_RWLOCK(lock)             \
    if (rwspin_lock_is_held_by_this_cpu(lock) == true)          \
    {                                                           \
        panic("debug assert failed: spin lock is held by CPU"); \
    }

#else

/// no-op
#define DEBUG_ASSERT_CPU_HOLDS_RWLOCK(lock)

// no-op
#define DEBUG_ASSERT_CPU_DOES_NOT_HOLD_RWLOCK(lock)

#endif  // CONFIG_DEBUG_SPINLOCK
