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
#include <lib/cbuffer.h>

#include "print_impl.h"

typedef void (*putc_func_t)(int32_t c);

/// lock to avoid interleaving concurrent printk's.
static struct
{
    struct spinlock lock;
    bool locking;
    bool init;
    putc_func_t putc_func;
    struct Circular_Buffer cb;
} g_printk = {0};

// store printk output until a console driver is available, then flush it to the
// console.
#define EARLY_PRINT_BUFFER_SIZE (512)
char g_early_printk_buffer[EARLY_PRINT_BUFFER_SIZE];

// default putc() callback before a driver was found, stores data for later
// dumping.
void console_none(int32_t c)
{
    cbuffer_write(&g_printk.cb, (const char *)&c, 1);
}

// print_impl compatible callback to print a char via the current putc()
// function.
void console_putc_dummy(int32_t c, size_t payload)
{
    putc_func_t func = (putc_func_t)payload;
    func(c);
}

void printk_init()
{
    spin_lock_init(&g_printk.lock, "pr");
    g_printk.locking = true;
    g_printk.init = true;
    g_printk.putc_func = console_none;
    cbuffer_init(&g_printk.cb, g_early_printk_buffer, EARLY_PRINT_BUFFER_SIZE);

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

    // flush early printk buffer to new console:
    size_t available_data = cbuffer_available_data(&g_printk.cb);
    size_t available_space = cbuffer_available_space(&g_printk.cb);
    if (available_data > 0)
    {
        char temp_buffer[EARLY_PRINT_BUFFER_SIZE];
        cbuffer_read(&g_printk.cb, temp_buffer, available_data);

        printk("Flushing early printk buffer to console:\n");
        if (available_space == 0)
        {
            printk("[lost data]\n%s", temp_buffer);
        }
        else
        {
            printk("%s", temp_buffer);
        }
    }
}

void printk_disable_locking() { g_printk.locking = false; }

// Print to the console
void printk(char *format, ...)
{
    if (g_printk.init == false)
    {
        return;
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
