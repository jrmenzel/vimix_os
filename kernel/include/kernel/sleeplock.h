/* SPDX-License-Identifier: MIT */
#pragma once

#include <kernel/spinlock.h>

/// Long-term locks for processes
struct sleeplock
{
    uint locked;         ///< Is the lock held?
    struct spinlock lk;  ///< spinlock protecting this sleep lock

    // For debugging:
    char *name;  ///< Name of lock.
    int pid;     ///< Process holding lock
};

void sleep_lock(struct sleeplock *);
void sleep_unlock(struct sleeplock *);
int sleep_lock_is_held_by_this_cpu(struct sleeplock *);
void sleep_lock_init(struct sleeplock *, char *);
