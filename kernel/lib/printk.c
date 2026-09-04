/* SPDX-License-Identifier: MIT */

//
// formatted console output -- printk
//

#ifdef __ARCH_riscv
#include <arch/riscv/sbi.h>
#include <arch/riscv/sbi_defs.h>
#endif

#include <drivers/tty/console.h>
#include <kernel/kernel.h>
#include <kernel/printk.h>
#include <kernel/spinlock.h>
#include <kernel/stdarg.h>
#include <kernel/types.h>
#include <lib/cbuffer.h>

#include "print_impl.h"

/// lock to avoid interleaving concurrent printk's.
struct PrintK
{
    struct spinlock lock;
    bool locking;
    bool init;
    struct Circular_Buffer cb;
    struct Console_Device *console;
};

struct PrintK g_printk = {0};

// store printk output until a console driver is available, then flush it to the
// console.
#define EARLY_PRINT_BUFFER_SIZE (1024)
char g_early_printk_buffer[EARLY_PRINT_BUFFER_SIZE];

// print_impl compatible callback to print a char via the current putc()
// function.
void console_putc_dummy(int32_t c, size_t payload)
{
    if (g_printk.console == NULL)
    {
        cbuffer_write(&g_printk.cb, (const char *)&c, 1);
    }
    else
    {
        console_putc(g_printk.console, c);
    }
}

void printk_init()
{
    spin_lock_init(&g_printk.lock, "pr");
    g_printk.locking = true;
    g_printk.init = true;
    g_printk.console = NULL;
    cbuffer_init(&g_printk.cb, g_early_printk_buffer, EARLY_PRINT_BUFFER_SIZE);
}

bool printk_has_console() { return (g_printk.console != NULL); }

void printk_set_console(struct Console_Device *console)
{
    printk("Redirecting printk to first console device...\n");
    g_printk.console = console;

    // flush early printk buffer to new console:
    size_t available_data = cbuffer_available_data(&g_printk.cb);
    size_t available_space = cbuffer_available_space(&g_printk.cb);
    if (available_data > 0)
    {
        printk("Flushing early printk buffer to console:\n");
        if (available_space == 0)
        {
            printk("[lost data]\n");
        }

        // Circular buffers contain bytes, not NUL-terminated strings. Flush
        // exactly the bytes that were present when the console was selected.
        for (size_t i = 0; i < available_data; ++i)
        {
            char c;
            cbuffer_read(&g_printk.cb, &c, 1);
            console_putc(console, c);
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

    // printing via console
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
