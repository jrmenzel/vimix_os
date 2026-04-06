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

void sleep_lock_2(struct sleeplock *lk0, struct sleeplock *lk1)
{
    DEBUG_EXTRA_PANIC(lk0 != lk1, "sleep_lock_2: both locks are the same");

    while (true)
    {
        if (sleep_trylock(lk0))
        {
            if (sleep_trylock(lk1))
            {
                // got both locks
                return;
            }
            // could not get second lock, unlock first and try again
            sleep_unlock(lk0);
        }
        yield();  // let other threads run
    }
}

bool sleep_trylock(struct sleeplock *lk)
{
    bool locked = spin_trylock(&lk->lk);
    if (locked == false) return false;

    if (lk->locked)
    {
        spin_unlock(&lk->lk);
        return false;
    }
    lk->locked = true;
#ifdef CONFIG_DEBUG_SLEEPLOCK
    lk->pid = get_current()->pid;
#endif  // CONFIG_DEBUG_SLEEPLOCK
    spin_unlock(&lk->lk);
    return true;
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
    // spin_lock(&lk->lk);
    bool rel = lk->locked && (lk->pid == get_current()->pid);
    // spin_unlock(&lk->lk);
    return rel;
}
#endif  // CONFIG_DEBUG_SLEEPLOCK
