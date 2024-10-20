/* SPDX-License-Identifier: MIT */

//
// Console input and output, to the uart.
// Reads are line at a time.
//

#include <drivers/character_device.h>
#include <drivers/console.h>
#include <drivers/uart16550.h>
#include <kernel/file.h>
#include <kernel/fs.h>
#include <kernel/kernel.h>
#include <kernel/major.h>
#include <kernel/proc.h>
#include <kernel/sleeplock.h>
#include <kernel/spinlock.h>

#define BACKSPACE 0x100
#define DELETE_KEY '\x7f'
#define CONTROL_KEY(x) ((x) - '@')  // Control-x

/// send one character to the uart.
/// called by printk(), and to echo input characters,
/// but not from write().
void console_putc(int32_t c)
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
    struct Character_Device cdev;  ///< derived from a character device
    struct Device_Memory_Map mapping;

    struct spinlock lock;

    // input
#define INPUT_BUF_SIZE 128
    char buf[INPUT_BUF_SIZE];
    uint32_t r;  ///< Read index
    uint32_t w;  ///< Write index
    uint32_t e;  ///< Edit index
} g_console;

/// user write()s to the console go here.
ssize_t console_write(struct Device *dev, bool addr_is_userspace, size_t src,
                      size_t n)
{
    ssize_t i;

    for (i = 0; i < (ssize_t)n; i++)
    {
        char c;
        if (either_copyin(&c, addr_is_userspace, src + i, 1) == -1)
        {
            break;
        }
        uart_putc(c);
    }

    return i;
}

/// user read()s from the console go here.
/// copy (up to) a whole input line to dst.
/// user_dist indicates whether dst is a user
/// or kernel address.
ssize_t console_read(struct Device *dev, bool addr_is_userspace, size_t dst,
                     size_t n)
{
    size_t target = n;

    spin_lock(&g_console.lock);
    while (n > 0)
    {
        // wait until interrupt handler has put some
        // input into g_console.buffer.
        while (g_console.r == g_console.w)
        {
            if (proc_is_killed(get_current()))
            {
                spin_unlock(&g_console.lock);
                return -1;
            }
            sleep(&g_console.r, &g_console.lock);
        }

        int32_t c = g_console.buf[g_console.r++ % INPUT_BUF_SIZE];

        if (c == CONTROL_KEY('D'))
        {  // end-of-file
            if (n < target)
            {
                // Save ^D for next time, to make sure
                // caller gets a 0-byte result.
                g_console.r--;
            }
            break;
        }

        // copy the input byte to the user-space buffer.
        char cbuf = c;
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
    spin_unlock(&g_console.lock);

    return target - n;
}

void console_debug_print_help()
{
    printk("\n");
    printk("CTRL+H: Print this help\n");
    printk("CTRL+N: Print inodes\n");
    printk("CTRL+P: Print process list\n");
    printk("CTRL+L: Print process list\n");
    printk("CTRL+T: Print process list with page tables\n");
    printk("CTRL+U: Print process list with user call stack\n");
    printk("CTRL+S: Print process list with kernel call stack\n");
    printk("CTRL+O: Print process list with open files\n");
}

/// the console input interrupt handler.
/// uart_interrupt_handler() calls this for input character.
/// do erase/kill processing, append to g_console.buf,
/// wake up console_read() if a whole line has arrived.
void console_interrupt_handler(int32_t c)
{
    spin_lock(&g_console.lock);

    switch (c)
    {
        case CONTROL_KEY('H'): console_debug_print_help(); break;
        case CONTROL_KEY('P'):  // Print process list.
        case CONTROL_KEY('L'):  // Print process _L_ist, alternative for VSCode
                                // which grabs CTRL+P
            debug_print_process_list(false, false, false, false);
            break;
        case CONTROL_KEY('T'):  // Process list with page _T_ables
            debug_print_process_list(false, false, false, true);
            break;
        case CONTROL_KEY('U'):  // Process list with _U_ser call stack
            debug_print_process_list(true, false, false, false);
            break;
        case CONTROL_KEY('S'):  // Process list with kernel call _S_tack
            debug_print_process_list(false, true, false, false);
            break;
        case CONTROL_KEY('O'):  // Process list with open files
            debug_print_process_list(false, false, true, false);
            break;
        case CONTROL_KEY('N'):  // print i_N_odes
            debug_print_inodes();
            break;
        case DELETE_KEY:  // Delete key
            if (g_console.e != g_console.w)
            {
                g_console.e--;
                console_putc(BACKSPACE);
            }
            break;
        default:
            if (c != 0 && g_console.e - g_console.r < INPUT_BUF_SIZE)
            {
                c = (c == '\r') ? '\n' : c;

                // echo back to the user.
                console_putc(c);

                // store for consumption by console_read().
                g_console.buf[g_console.e++ % INPUT_BUF_SIZE] = c;

                if (c == '\n' || c == CONTROL_KEY('D') ||
                    g_console.e - g_console.r == INPUT_BUF_SIZE)
                {
                    // wake up console_read() if a whole line (or end-of-file)
                    // has arrived.
                    g_console.w = g_console.e;
                    wakeup(&g_console.r);
                }
            }
            break;
    }

    spin_unlock(&g_console.lock);
}

dev_t console_init(struct Device_Memory_Map *dev_map)
{
    spin_lock_init(&g_console.lock, "cons");

    uart_init(dev_map);

    // init device and register it in the system
    g_console.cdev.dev.type = CHAR;
    g_console.cdev.dev.device_number = MKDEV(CONSOLE_DEVICE_MAJOR, 0);
    g_console.cdev.ops.read = console_read;
    g_console.cdev.ops.write = console_write;
    dev_set_irq(&g_console.cdev.dev, dev_map->interrupt,
                uart_interrupt_handler);
    register_device(&g_console.cdev.dev);

    return g_console.cdev.dev.device_number;
}
