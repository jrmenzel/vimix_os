/* SPDX-License-Identifier: MIT */

//
// Console input and output, to the uart.
// Reads are line at a time.
//

#include <arch/riscv/sbi.h>
#include <drivers/character_device.h>
#include <drivers/console.h>
#include <drivers/htif.h>
#include <drivers/uart16550.h>
#include <kernel/file.h>
#include <kernel/fs.h>
#include <kernel/kernel.h>
#include <kernel/major.h>
#include <kernel/proc.h>
#include <kernel/sleeplock.h>
#include <kernel/spinlock.h>
#include <kernel/string.h>

#define BACKSPACE 0x100
#define DELETE_KEY '\x7f'
#define CONTROL_KEY(x) ((x) - '@')  // Control-x

// pointers for the put chat function of either a UART driver or the SBI
// fallback
void (*device_putc)(int32_t ch) = NULL;
void (*device_putc_sync)(int32_t ch) = NULL;

// true if the SBI fallback is used
bool g_console_poll_sbi = false;

/// send one character to the uart.
/// called by printk(), and to echo input characters,
/// but not from write().
void console_putc(int32_t c)
{
    if (device_putc_sync == NULL) return;

    if (c == BACKSPACE)
    {
        // if the user typed backspace, overwrite with a space.
        device_putc_sync('\b');
        device_putc_sync(' ');
        device_putc_sync('\b');
    }
    else
    {
        device_putc_sync(c);
    }
}

struct
{
    struct Character_Device cdev;  ///< derived from a character device
    struct Device_Init_Parameters init_parameters;

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

        device_putc(c);
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

void console_putc_noop(int32_t ch) {}

dev_t console_init(struct Device_Init_Parameters *init_param, const char *name)
{
    spin_lock_init(&g_console.lock, "cons");

    // init device and register it in the system
    g_console.cdev.dev.name = "console";
    g_console.cdev.dev.type = CHAR;
    g_console.cdev.dev.device_number = MKDEV(CONSOLE_DEVICE_MAJOR, 0);
    g_console.cdev.ops.read = console_read;
    g_console.cdev.ops.write = console_write;
    g_console.cdev.dev.irq_number = INVALID_IRQ_NUMBER;

    if (init_param != NULL)
    {
        if (strcmp(name, "ucb,htif0") == 0)
        {
            device_putc = htif_putc;
            device_putc_sync = htif_putc;

            htif_init(init_param, name);
        }
        else
        {
            // ns16550a or snps,dw-apb-uart
            device_putc = uart_putc;
            device_putc_sync = uart_putc_sync;

            uart_init(init_param, name);
            dev_set_irq(&g_console.cdev.dev, init_param->interrupt,
                        uart_interrupt_handler);
        }
        g_console_poll_sbi = false;
    }
    else
    {
#ifdef __ENABLE_SBI__
        if (sbi_probe_extension(SBI_LEGACY_EXT_CONSOLE_PUTCHAR) <= 0)
        {
            panic(
                "Error: support for the SBI legacy console extension "
                "missing\n");
        }

        // SBI console fallback
        device_putc = sbi_console_putchar;
        device_putc_sync = sbi_console_putchar;
        g_console_poll_sbi = true;  // there is no IRQ, this var is a hack to
                                    // poll the input via the timer
#else
        // run with no input/output:
        device_putc = console_putc_noop;
        device_putc_sync = console_putc_noop;
#endif
    }

    register_device(&g_console.cdev.dev);

    return g_console.cdev.dev.device_number;
}
