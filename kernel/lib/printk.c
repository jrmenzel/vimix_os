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

volatile int g_kernel_panicked = 0;

/// lock to avoid interleaving concurrent printk's.
static struct
{
    struct spinlock lock;
    int locking;
} pr;

static char digits[] = "0123456789abcdef";

static void printint(int xx, int base, int sign)
{
    char buf[16];
    int i;
    uint x;

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

static void printptr(uint64 x)
{
    int i;
    console_putc('0');
    console_putc('x');
    for (i = 0; i < (sizeof(uint64) * 2); i++, x <<= 4)
        console_putc(digits[x >> (sizeof(uint64) * 8 - 4)]);
}

/// Print to the console. only understands %d, %x, %p, %s.
void printk(char *fmt, ...)
{
    va_list ap;
    int i, c, locking;
    char *s;

    locking = pr.locking;
    if (locking) spin_lock(&pr.lock);

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
            case 'p': printptr(va_arg(ap, uint64)); break;
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

    if (locking) spin_unlock(&pr.lock);
}

void panic(char *s)
{
    pr.locking = 0;
    printk("panic: ");
    printk(s);
    printk("\n");
    g_kernel_panicked = 1;  // freeze uart output from other CPUs
    for (;;)
        ;
}

void printk_init()
{
    spin_lock_init(&pr.lock, "pr");
    pr.locking = 1;
}
