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

void acquire(struct spinlock *);
int holding(struct spinlock *);
void initlock(struct spinlock *, char *);
void release(struct spinlock *);
