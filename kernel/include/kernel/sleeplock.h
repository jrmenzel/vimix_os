/* SPDX-License-Identifier: MIT */
#pragma once

#include <kernel/spinlock.h>
#include <kernel/types.h>

/// Long-term locks for processes
struct sleeplock
{
    uint32_t locked;     ///< Is the lock held?
    struct spinlock lk;  ///< spinlock protecting this sleep lock

#ifdef CONFIG_DEBUG_SLEEPLOCK
    pid_t pid;   ///< Process holding this lock
    char *name;  ///< For debugging: Name of lock.
#endif           // CONFIG_DEBUG_SLEEPLOCK
};

void sleep_lock_init(struct sleeplock *lk, char *name_for_debug);

static inline void sleep_lock_init_locked(struct sleeplock *lk,
                                          char *name_for_debug)
{
    sleep_lock_init(lk, name_for_debug);
    lk->locked = 1;
}

void sleep_lock(struct sleeplock *lk);

void sleep_lock_2(struct sleeplock *lk0, struct sleeplock *lk1);

/// @brief Returns true if the lock was aquired, does not block, does not sleep.
/// @param lk The lock.
/// @return True if the lock was aquired.
bool sleep_trylock(struct sleeplock *lk);

void sleep_unlock(struct sleeplock *lk);

static inline void sleep_unlock_2(struct sleeplock *lk0, struct sleeplock *lk1)
{
    sleep_unlock(lk0);
    sleep_unlock(lk1);
}

#ifdef CONFIG_DEBUG_SLEEPLOCK
bool sleep_lock_is_held_by_this_cpu(struct sleeplock *lk);
#endif  // CONFIG_DEBUG_SLEEPLOCK
