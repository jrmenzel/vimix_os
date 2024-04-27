/* SPDX-License-Identifier: MIT */

//
// formatted console output -- printk, panic.
//

#include <kernel/stdarg.h>

#include <kernel/console.h>
#include <kernel/file.h>
#include <kernel/fs.h>
#include <kernel/param.h>
#include <kernel/printk.h>
#include <kernel/proc.h>
#include <kernel/sleeplock.h>
#include <kernel/spinlock.h>
#include <kernel/types.h>
#include <mm/memlayout.h>

volatile bool g_kernel_panicked = false;

/// lock to avoid interleaving concurrent printk's.
static struct
{
    struct spinlock lock;
    bool locking;
} g_printk;

static char digits[] = "0123456789abcdef";

static void printint(int xx, int base, int sign)
{
    char buf[16];
    int i;
    uint32_t x;

    if (sign && (sign = xx < 0))
        x = -xx;
    else
        x = xx;

    i = 0;
    do {
        buf[i++] = digits[x % base];
    } while ((x /= base) != 0);

    if (sign) buf[i++] = '-';

    while (--i >= 0) console_putc(buf[i]);
}

static void printptr(uint64_t x)
{
    int i;
    console_putc('0');
    console_putc('x');
    for (i = 0; i < (sizeof(uint64_t) * 2); i++, x <<= 4)
        console_putc(digits[x >> (sizeof(uint64_t) * 8 - 4)]);
}

/// Print to the console. only understands %d, %x, %p, %s.
void printk(char *fmt, ...)
{
    va_list ap;
    int i, c, locking;
    char *s;

    locking = g_printk.locking;
    if (locking) spin_lock(&g_printk.lock);

    if (fmt == 0) panic("null fmt");

    va_start(ap, fmt);
    for (i = 0; (c = fmt[i] & 0xff) != 0; i++)
    {
        if (c != '%')
        {
            console_putc(c);
            continue;
        }
        c = fmt[++i] & 0xff;
        if (c == 0) break;
        switch (c)
        {
            case 'd': printint(va_arg(ap, int), 10, 1); break;
            case 'x': printint(va_arg(ap, int), 16, 1); break;
            case 'p': printptr(va_arg(ap, uint64_t)); break;
            case 's':
                if ((s = va_arg(ap, char *)) == 0) s = "(null)";
                for (; *s; s++) console_putc(*s);
                break;
            case '%': console_putc('%'); break;
            default:
                // Print unknown % sequence to draw attention.
                console_putc('%');
                console_putc(c);
                break;
        }
    }
    va_end(ap);

    if (locking) spin_unlock(&g_printk.lock);
}

void panic(char *s)
{
    g_printk.locking = false;
    printk("panic: ");
    printk(s);
    printk("\n");
    g_kernel_panicked = true;  // freeze uart output from other CPUs
    while (true)
    {
    }
}

void printk_init()
{
    spin_lock_init(&g_printk.lock, "pr");
    g_printk.locking = 1;
}
