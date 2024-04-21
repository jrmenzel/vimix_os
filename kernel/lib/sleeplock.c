/* SPDX-License-Identifier: MIT */

// Sleeping locks

#include <kernel/param.h>
#include <kernel/printk.h>
#include <kernel/proc.h>
#include <kernel/sleeplock.h>
#include <kernel/spinlock.h>
#include <kernel/types.h>
#include <mm/memlayout.h>

void sleep_lock_init(struct sleeplock *lk, char *name)
{
    spin_lock_init(&lk->lk, "sleep lock");
    lk->name = name;
    lk->locked = 0;
    lk->pid = 0;
}

void sleep_lock(struct sleeplock *lk)
{
    spin_lock(&lk->lk);
    while (lk->locked)
    {
        sleep(lk, &lk->lk);
    }
    lk->locked = 1;
    lk->pid = get_current()->pid;
    spin_unlock(&lk->lk);
}

void sleep_unlock(struct sleeplock *lk)
{
    spin_lock(&lk->lk);
    lk->locked = 0;
    lk->pid = 0;
    wakeup(lk);
    spin_unlock(&lk->lk);
}

int sleep_lock_is_held_by_this_cpu(struct sleeplock *lk)
{
    int r;

    spin_lock(&lk->lk);
    r = lk->locked && (lk->pid == get_current()->pid);
    spin_unlock(&lk->lk);
    return r;
}
