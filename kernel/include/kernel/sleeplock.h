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
void sleep_lock(struct sleeplock *lk);
void sleep_unlock(struct sleeplock *lk);

#ifdef CONFIG_DEBUG_SLEEPLOCK
bool sleep_lock_is_held_by_this_cpu(struct sleeplock *lk);
#endif  // CONFIG_DEBUG_SLEEPLOCK
