/* SPDX-License-Identifier: MIT */

// Sleeping locks

#include <kernel/kernel.h>
#include <kernel/proc.h>
#include <kernel/sleeplock.h>
#include <kernel/spinlock.h>

void sleep_lock_init(struct sleeplock *lk, char *name_for_debug)
{
    spin_lock_init(&lk->lk, "sleep lock");
    lk->locked = false;

#ifdef CONFIG_DEBUG_SLEEPLOCK
    lk->pid = 0;
    lk->name = name_for_debug;
#endif  // CONFIG_DEBUG_SLEEPLOCK
}

void sleep_lock(struct sleeplock *lk)
{
    spin_lock(&lk->lk);
    while (lk->locked)
    {
        sleep(lk, &lk->lk);
    }
    lk->locked = true;
#ifdef CONFIG_DEBUG_SLEEPLOCK
    lk->pid = get_current()->pid;
#endif  // CONFIG_DEBUG_SLEEPLOCK
    spin_unlock(&lk->lk);
}

void sleep_unlock(struct sleeplock *lk)
{
    spin_lock(&lk->lk);
    lk->locked = false;
#ifdef CONFIG_DEBUG_SLEEPLOCK
    lk->pid = 0;
#endif  // CONFIG_DEBUG_SLEEPLOCK
    wakeup(lk);
    spin_unlock(&lk->lk);
}

#ifdef CONFIG_DEBUG_SLEEPLOCK
bool sleep_lock_is_held_by_this_cpu(struct sleeplock *lk)
{
    spin_lock(&lk->lk);
    bool rel = lk->locked && (lk->pid == get_current()->pid);
    spin_unlock(&lk->lk);
    return rel;
}
#endif  // CONFIG_DEBUG_SLEEPLOCK
