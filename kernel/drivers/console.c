/* SPDX-License-Identifier: MIT */

//
// Console input and output, to the uart.
// Reads are line at a time.
//

#include <arch/riscv/sbi.h>
#include <arch/timer.h>
#include <drivers/character_device.h>
#include <drivers/console.h>
#include <drivers/htif.h>
#include <drivers/uart16550.h>
#include <kernel/errno.h>
#include <kernel/file.h>
#include <kernel/fs.h>
#include <kernel/ioctl.h>
#include <kernel/kernel.h>
#include <kernel/kticks.h>
#include <kernel/major.h>
#include <kernel/proc.h>
#include <kernel/sleeplock.h>
#include <kernel/spinlock.h>
#include <kernel/string.h>
#include <kernel/termios.h>

#define BACKSPACE 0x100
#define DELETE_KEY '\x7f'
#define CONTROL_KEY(x) ((x) - '@')  // Control-x

// pointers for the put chat function of either a UART driver or the SBI
// fallback
void (*device_putc)(int32_t ch) = NULL;
void (*device_putc_sync)(int32_t ch) = NULL;

/// @brief NULL if no regular callback to poll input is needed
/// (used if no real console device is available)
void (*g_console_poll_callback)() = NULL;

/// @brief add a CR / 'r' for every '\n' written
const bool g_console_add_cr = true;

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
        if (g_console_add_cr && c == '\n')
        {
            // add carrige return to a newline
            device_putc_sync('\r');
        }
        device_putc_sync(c);
    }
}

struct
{
    struct Character_Device cdev;  ///< derived from a character device
    struct Device_Init_Parameters init_parameters;

    struct spinlock lock;

    /// max line length: real UNIXes allow 4096 bytes
#define INPUT_BUF_SIZE 128
    char buf[INPUT_BUF_SIZE];
    uint32_t r;  ///< Read index
    uint32_t w;  ///< Write index
    uint32_t e;  ///< Edit index

    // support for simple RAW mode
    struct termios termios;
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

        if (g_console_add_cr && c == '\n')
        {
            // add carrige return to a newline, don't count extra
            device_putc('\r');
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
                     size_t n, uint32_t unused_file_offset)
{
    size_t target = n;
    ssize_t termios_target = g_console.termios.c_cc[VMIN];
    bool canonical_mode = (g_console.termios.c_lflag & ICANON);

    spin_lock(&g_console.lock);
    while (n > 0)
    {
        size_t timeout = g_console.termios.c_cc[VTIME];  // 1/10s
        timeout = timeout * TIMER_INTERRUPTS_PER_SECOND / 10;
        timeout += kticks_get_ticks();

        // wait until interrupt handler has put some
        // input into g_console.buffer.
        while (g_console.r == g_console.w)
        {
            if (proc_is_killed(get_current()))
            {
                spin_unlock(&g_console.lock);
                return -ESRCH;
            }
            if (canonical_mode)
            {
                sleep(&g_console.r, &g_console.lock);
            }
            else
            {
                size_t now = kticks_get_ticks();
                if (now >= timeout)
                {
                    // timeout expired
                    spin_unlock(&g_console.lock);
                    return 0;
                }
                // wake up eack kernel tick to check for input
                // if we wait here for a console interrupt, we miss the
                // timeout
                sleep(&g_ticks, &g_console.lock);
            }
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
        --termios_target;

        if (canonical_mode)
        {
            if (c == '\n')
            {
                // a whole line has arrived, return to
                // the user-level read().
                break;
            }
        }
        else if (termios_target <= 0)
        {
            break;
        }
    }
    spin_unlock(&g_console.lock);

    return target - n;
}

int console_ioctl(struct inode *ip, int req, void *ttyctl)
{
    spin_lock(&g_console.lock);
    if (req == TCGETA)
    {
        // *termios_p = cons.termios;
        struct termios *termios_out = (struct termios *)ttyctl;

        if (either_copyout(true, (size_t)termios_out,
                           (void *)&(g_console.termios),
                           sizeof(struct termios)) == -1)
        {
            spin_unlock(&g_console.lock);
            return -1;
        }
    }
    else if (req == TCSETA)
    {
        // cons.termios = *termios_p;
        struct termios *termios_in = (struct termios *)ttyctl;

        if (either_copyin((void *)&(g_console.termios), true,
                          (size_t)termios_in, sizeof(struct termios)) == -1)
        {
            spin_unlock(&g_console.lock);
            return -1;
        }
    }
    else if (req == TIOCGWINSZ)
    {
        struct winsize *ws_out = (struct winsize *)ttyctl;

        struct winsize ws;
        ws.ws_col = 80;
        ws.ws_row = 24;
        ws.ws_xpixel = ws.ws_col * 8;
        ws.ws_ypixel = ws.ws_col * 16;

        if (either_copyout(true, (size_t)(ws_out), (void *)&(ws),
                           sizeof(struct winsize)) == -1)
        {
            spin_unlock(&g_console.lock);
            return -1;
        }
    }
    else
    {
        printk("console_ioctl: unknown request 0x%x\n", req);
        spin_unlock(&g_console.lock);
        return -1;
    }
    spin_unlock(&g_console.lock);
    return 0;
}

void console_debug_print_help()
{
    printk("\n");
    printk("CTRL+H: Print this help\n");
    printk("CTRL+N: Print inodes\n");
    printk("CTRL+P: Print process list\n");
    printk("CTRL+L: Print process list\n");
    printk("CTRL+T: Print process list with page tables\n");
    printk("CTRL+B: Print kernel page table (warning, long!)\n");
    printk("CTRL+U: Print process list with user call stack\n");
    printk("CTRL+S: Print process list with kernel call stack\n");
    printk("CTRL+O: Print process list with open files\n");
}

bool console_handle_control_keys(int32_t c)
{
    bool processed = true;

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
        case CONTROL_KEY('B'):  // kernel page table - running out of memorable
                                // key combos don't collide with VSCode
            printk("Kernel process table:\n");
            debug_vm_print_page_table(g_kernel_pagetable);
            break;
        case DELETE_KEY:  // Delete key
            if (g_console.e != g_console.w)
            {
                g_console.e--;
                if (g_console.termios.c_lflag & ECHO)
                {
                    console_putc(BACKSPACE);
                }
            }
            break;
        default: processed = false; break;
    }

    return processed;
}

/// the console input interrupt handler.
/// uart_interrupt_handler() calls this for input character.
/// do erase/kill processing, append to g_console.buf,
/// wake up console_read() if a whole line has arrived.
void console_interrupt_handler(int32_t c)
{
    spin_lock(&g_console.lock);

    bool input_processed = false;
    if (g_console.termios.c_lflag & ICANON)
    {
        input_processed = console_handle_control_keys(c);
    }

    if (!input_processed)
    {
        if (c != 0 && g_console.e - g_console.r < INPUT_BUF_SIZE)
        {
            // carriage return to newline
            if (g_console.termios.c_lflag & ICRNL)
            {
                c = (c == '\r') ? '\n' : c;
            }

            // echo back to the user.
            if (g_console.termios.c_lflag & ECHO)
            {
                console_putc(c);
            }

            // store for consumption by console_read().
            g_console.buf[g_console.e++ % INPUT_BUF_SIZE] = c;

            // in non-canonical mode, return each key press, otherwise wait for
            // newline
            bool wakeup_readers = !(g_console.termios.c_lflag & ICANON);
            wakeup_readers |= (c == '\n');
            wakeup_readers |= (c == CONTROL_KEY('D'));
            // buffer full:
            wakeup_readers |= (g_console.e - g_console.r == INPUT_BUF_SIZE);

            if (wakeup_readers)
            {
                g_console.w = g_console.e;
                wakeup(&g_console.r);
            }
        }
    }

    spin_unlock(&g_console.lock);
}

void console_putc_noop(int32_t ch) {}

dev_t console_init(struct Device_Init_Parameters *init_param, const char *name)
{
    // already initialized with another device
    if (device_putc != NULL)
    {
        return INVALID_DEVICE;
    }

    struct Devices_List *dev_list = get_devices_list();
    dev_t uart_dev = init_device_by_name(dev_list, name);
    if (uart_dev == INVALID_DEVICE) return INVALID_DEVICE;

    spin_lock_init(&g_console.lock, "cons");

    // init device and register it in the system
    g_console.cdev.dev.name = "console";
    g_console.cdev.dev.type = CHAR;
    g_console.cdev.dev.device_number = MKDEV(CONSOLE_DEVICE_MAJOR, 0);
    g_console.cdev.ops.read = console_read;
    g_console.cdev.ops.write = console_write;
    g_console.cdev.ops.ioctl = console_ioctl;
    g_console.cdev.dev.irq_number = INVALID_IRQ_NUMBER;

    memset(&g_console.termios, sizeof(struct termios), 0);
    g_console.termios.c_lflag = ECHO | ICANON | ICRNL;
    g_console.termios.c_cc[VMIN] = 1;   // read() blocks for at least one byte
    g_console.termios.c_cc[VTIME] = 0;  // no timeout in read()

    if (init_param != NULL)
    {
        if (strcmp(name, "ucb,htif0") == 0)
        {
            device_putc = htif_putc;
            device_putc_sync = htif_putc;
            g_console_poll_callback = htif_console_poll_input;

            // htif_init(init_param, name);
        }
        else
        {
            // ns16550a or snps,dw-apb-uart
            device_putc = uart_putc;
            device_putc_sync = uart_putc_sync;

            // uart_init(init_param, name);
            dev_set_irq(&g_console.cdev.dev, init_param->interrupt,
                        uart_interrupt_handler);
        }
    }
    else
    {
#ifdef ARCH_riscv
        if (sbi_probe_extension(SBI_LEGACY_EXT_CONSOLE_PUTCHAR) > 0)
        {
            // SBI console fallback
            device_putc = sbi_console_putchar;
            device_putc_sync = sbi_console_putchar;
            g_console_poll_callback = sbi_console_poll_input;
        }
        else
#endif
        {
            // run with no input/output:
            device_putc = console_putc_noop;
            device_putc_sync = console_putc_noop;
        }
    }

    register_device(&g_console.cdev.dev);

    return g_console.cdev.dev.device_number;
}
