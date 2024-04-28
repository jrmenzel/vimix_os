/* SPDX-License-Identifier: MIT */

//
// formatted console output -- printk, panic.
//

#include <kernel/printk.h>

#include <kernel/console.h>
#include <kernel/spinlock.h>
#include <kernel/stdarg.h>
#include <kernel/types.h>

#include "print_impl.h"

volatile bool g_kernel_panicked = false;

/// lock to avoid interleaving concurrent printk's.
static struct
{
    struct spinlock lock;
    bool locking;
} g_printk;

void console_putc_dummy(int32_t c, size_t payload) { console_putc(c); }

// Print to the console
void printk(char *format, ...)
{
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

void panic(char *error_message)
{
    g_printk.locking = false;
    printk("panic: ");
    printk(error_message);
    printk("\n");
    g_kernel_panicked = true;  // freeze uart output from other CPUs
    while (true)
    {
    }
}

void printk_init()
{
    spin_lock_init(&g_printk.lock, "pr");
    g_printk.locking = true;
}
