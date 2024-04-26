/* SPDX-License-Identifier: MIT */

//
// Console input and output, to the uart.
// Reads are line at a time.
// Implements special input characters:
//   newline -- end of line
//   control-h -- backspace
//   control-u -- kill line
//   control-d -- end of file
//   control-p -- print process list
//

#include <drivers/uart16550.h>
#include <kernel/file.h>
#include <kernel/fs.h>
#include <kernel/kernel.h>
#include <kernel/proc.h>
#include <kernel/sleeplock.h>
#include <kernel/spinlock.h>
#include <mm/memlayout.h>

#define BACKSPACE 0x100
#define CONTROL_KEY(x) ((x) - '@')  // Control-x

/// send one character to the uart.
/// called by printk(), and to echo input characters,
/// but not from write().
void console_putc(int c)
{
    if (c == BACKSPACE)
    {
        // if the user typed backspace, overwrite with a space.
        uart_putc_sync('\b');
        uart_putc_sync(' ');
        uart_putc_sync('\b');
    }
    else
    {
        uart_putc_sync(c);
    }
}

struct
{
    struct spinlock lock;

    // input
#define INPUT_BUF_SIZE 128
    char buf[INPUT_BUF_SIZE];
    uint32_t r;  ///< Read index
    uint32_t w;  ///< Write index
    uint32_t e;  ///< Edit index
} cons;

/// user write()s to the console go here.
int console_write(int addr_is_userspace, uint64_t src, int n)
{
    int i;

    for (i = 0; i < n; i++)
    {
        char c;
        if (either_copyin(&c, addr_is_userspace, src + i, 1) == -1) break;
        uart_putc(c);
    }

    return i;
}

/// user read()s from the console go here.
/// copy (up to) a whole input line to dst.
/// user_dist indicates whether dst is a user
/// or kernel address.
int console_read(int addr_is_userspace, uint64_t dst, int n)
{
    uint32_t target;
    int c;
    char cbuf;

    target = n;
    spin_lock(&cons.lock);
    while (n > 0)
    {
        // wait until interrupt handler has put some
        // input into cons.buffer.
        while (cons.r == cons.w)
        {
            if (proc_is_killed(get_current()))
            {
                spin_unlock(&cons.lock);
                return -1;
            }
            sleep(&cons.r, &cons.lock);
        }

        c = cons.buf[cons.r++ % INPUT_BUF_SIZE];

        if (c == CONTROL_KEY('D'))
        {  // end-of-file
            if (n < target)
            {
                // Save ^D for next time, to make sure
                // caller gets a 0-byte result.
                cons.r--;
            }
            break;
        }

        // copy the input byte to the user-space buffer.
        cbuf = c;
        if (either_copyout(addr_is_userspace, dst, &cbuf, 1) == -1) break;

        dst++;
        --n;

        if (c == '\n')
        {
            // a whole line has arrived, return to
            // the user-level read().
            break;
        }
    }
    spin_unlock(&cons.lock);

    return target - n;
}

/// the console input interrupt handler.
/// uart_interrupt_handler() calls this for input character.
/// do erase/kill processing, append to cons.buf,
/// wake up console_read() if a whole line has arrived.
void console_interrupt_handler(int c)
{
    spin_lock(&cons.lock);

    switch (c)
    {
        case CONTROL_KEY('P'):  // Print process list.
            debug_print_process_list();
            break;
        case CONTROL_KEY('U'):  // Kill line.
            while (cons.e != cons.w &&
                   cons.buf[(cons.e - 1) % INPUT_BUF_SIZE] != '\n')
            {
                cons.e--;
                console_putc(BACKSPACE);
            }
            break;
        case CONTROL_KEY('H'):  // Backspace
        case '\x7f':            // Delete key
            if (cons.e != cons.w)
            {
                cons.e--;
                console_putc(BACKSPACE);
            }
            break;
        default:
            if (c != 0 && cons.e - cons.r < INPUT_BUF_SIZE)
            {
                c = (c == '\r') ? '\n' : c;

                // echo back to the user.
                console_putc(c);

                // store for consumption by console_read().
                cons.buf[cons.e++ % INPUT_BUF_SIZE] = c;

                if (c == '\n' || c == CONTROL_KEY('D') ||
                    cons.e - cons.r == INPUT_BUF_SIZE)
                {
                    // wake up console_read() if a whole line (or end-of-file)
                    // has arrived.
                    cons.w = cons.e;
                    wakeup(&cons.r);
                }
            }
            break;
    }

    spin_unlock(&cons.lock);
}

void console_init()
{
    spin_lock_init(&cons.lock, "cons");

    uart_init();

    // connect read and write system calls
    // to console_read and console_write.
    devsw[CONSOLE].read = console_read;
    devsw[CONSOLE].write = console_write;
}
