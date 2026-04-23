/* SPDX-License-Identifier: MIT */

//
// formatted console output -- printk
//

#ifdef __ARCH_riscv
#include <arch/riscv/sbi.h>
#endif

#include <drivers/console.h>
#include <kernel/kernel.h>
#include <kernel/printk.h>
#include <kernel/spinlock.h>
#include <kernel/stdarg.h>
#include <kernel/types.h>

#include "print_impl.h"

typedef void (*putc_func_t)(int32_t c);

/// lock to avoid interleaving concurrent printk's.
static struct
{
    struct spinlock lock;
    bool locking;
    bool init;
    putc_func_t putc_func;
} g_printk = {0};

void console_none(int32_t c) {}
void console_putc_dummy(int32_t c, size_t payload)
{
    putc_func_t func = (putc_func_t)payload;
    func(c);
}

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
    print_impl(console_putc_dummy, (size_t)g_printk.putc_func, format, ap);
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
    g_printk.putc_func = console_none;

#ifdef __ARCH_riscv
    if (sbi_probe_extension(SBI_LEGACY_EXT_CONSOLE_PUTCHAR) > 0)
    {
        // SBI console fallback
        g_printk.putc_func = sbi_console_putchar;
        printk("Early console detected: SBI\n");
    }
#endif
}

void printk_redirect_to_console()
{
    printk("Redirecting printk to console device...\n");
    g_printk.putc_func = console_putc;
}
