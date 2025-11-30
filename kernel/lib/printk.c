/* SPDX-License-Identifier: MIT */

//
// formatted console output -- printk
//

#include <drivers/console.h>
#include <kernel/kernel.h>
#include <kernel/printk.h>
#include <kernel/spinlock.h>
#include <kernel/stdarg.h>
#include <kernel/types.h>

#include "print_impl.h"

/// lock to avoid interleaving concurrent printk's.
static struct
{
    struct spinlock lock;
    bool locking;
    bool init;
} g_printk = {0};

void console_putc_dummy(int32_t c, size_t payload) { console_putc(c); }

void printk_disable_locking() { g_printk.locking = false; }

// Print to the console
void printk(char *format, ...)
{
    if (g_printk.init == false)
    {
        // return;
    }

    // error checks
    if (format == NULL)
    {
        panic("null format in printk");
    }

    // lock
    bool locking = g_printk.locking;
    if (locking)
    {
        spin_lock(&g_printk.lock);
    }

    // printing via console_putc()
    va_list ap;
    va_start(ap, format);
    print_impl(console_putc_dummy, 0, format, ap);
    va_end(ap);

    // unlock
    if (locking)
    {
        spin_unlock(&g_printk.lock);
    }
}

void printk_init()
{
    spin_lock_init(&g_printk.lock, "pr");
    g_printk.locking = true;
    g_printk.init = true;
}
