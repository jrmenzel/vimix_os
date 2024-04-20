/* SPDX-License-Identifier: MIT */
/*
 * Based on xv6.
 */

#include <kernel/cpu.h>
#include <kernel/printk.h>
#include <kernel/proc.h>

void push_off(void)
{
    int old = intr_get();

    intr_off();
    if (mycpu()->noff == 0) mycpu()->intena = old;
    mycpu()->noff += 1;
}

void pop_off(void)
{
    struct cpu *c = mycpu();
    if (intr_get()) panic("pop_off - interruptible");
    if (c->noff < 1) panic("pop_off");
    c->noff -= 1;
    if (c->noff == 0 && c->intena) intr_on();
}
