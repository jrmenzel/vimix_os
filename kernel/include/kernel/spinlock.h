/* SPDX-License-Identifier: MIT */
#pragma once

#include <kernel/kernel.h>

/// Mutual exclusion lock.
struct spinlock
{
    uint locked;  ///< Is the lock held?

    // For debugging:
    char *name;       ///< Name of lock.
    struct cpu *cpu;  ///< The cpu holding the lock.
};

void spin_lock(struct spinlock *);
int spin_lock_is_held_by_this_cpu(struct spinlock *);
void spin_lock_init(struct spinlock *, char *);
void spin_unlock(struct spinlock *);
