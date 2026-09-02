/* SPDX-License-Identifier: MIT */

//
// Console input and output, to the uart.
// Reads are line at a time.
//

#ifdef __ARCH_riscv
#include <arch/riscv/sbi.h>
#include <arch/riscv/sbi_defs.h>
#endif

#include <arch/irq.h>
#include <arch/riscv/sbi.h>
#include <drivers/driver.h>
#include <drivers/tty/arm_pl011.h>
#include <drivers/tty/bcm2835_aux_uart.h>
#include <drivers/tty/console.h>
#include <drivers/tty/htif.h>
#include <drivers/tty/uart16550.h>
#include <fs/dentry_cache.h>
#include <kernel/container_of.h>
#include <kernel/errno.h>
#include <kernel/file.h>
#include <kernel/fs.h>
#include <kernel/ioctl.h>
#include <kernel/kobject.h>
#include <kernel/kticks.h>
#include <kernel/proc.h>
#include <kernel/sleeplock.h>
#include <kernel/spinlock.h>
#include <kernel/string.h>
#include <kernel/termios.h>
#include <kernel/timer.h>

#define BACKSPACE 0x100
#define DELETE_KEY '\x7f'
#define CONTROL_KEY(x) ((x) - '@')  // Control-x

/// max line length: real UNIXes allow 4096 bytes
#define CONSOLE_INPUT_BUF_SIZE 128
struct Console_Device
{
    struct Character_Device cdev;  ///< derived from a character device
    struct Device_Init_Parameters init_parameters;

    struct spinlock lock;

    char buf[CONSOLE_INPUT_BUF_SIZE];
    size_t r;  ///< Read index
    size_t w;  ///< Write index
    size_t e;  ///< Edit index

    // support for simple RAW mode
    struct termios termios;

    bool add_cr;  ///< add a CR / 'r' for every '\n' written

    device_putc_fn putc;
    device_putc_fn putc_sync;
};

#define console_driver_from_cdev(ptr) \
    container_of((ptr), struct Console_Device, cdev)

/// @brief NULL if no regular callback to poll input is needed
/// (used if no real console device is available)
void (*g_console_poll_callback)() = NULL;

atomic_size_t g_console_next_minor = 0;

struct Console_Device *g_boot_console = NULL;

/// send one character to the uart.
/// called by printk(), and to echo input characters,
/// but not from write().
void console_putc(struct Console_Device *console, int32_t c)
{
    if (c == BACKSPACE)
    {
        // if the user typed backspace, overwrite with a space.
        console->putc_sync('\b');
        console->putc_sync(' ');
        console->putc_sync('\b');
    }
    else
    {
        if (console->add_cr && c == '\n')
        {
            // add carrige return to a newline
            console->putc_sync('\r');
        }
        console->putc_sync(c);
    }
}

/// user write()s to the console go here.
ssize_t console_write(struct Device *dev, bool addr_is_userspace, size_t src,
                      size_t n)
{
    struct Character_Device *cdev = character_device_from_device(dev);
    struct Console_Device *console = console_driver_from_cdev(cdev);

    ssize_t i;
    for (i = 0; i < (ssize_t)n; i++)
    {
        char c;
        if (either_copyin(&c, addr_is_userspace, src + i, 1) == -1)
        {
            break;
        }

        if (console->add_cr && c == '\n')
        {
            // add carrige return to a newline, don't count extra
            console->putc('\r');
        }
        console->putc(c);
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
    struct Character_Device *cdev = character_device_from_device(dev);
    struct Console_Device *console = console_driver_from_cdev(cdev);

    size_t target = n;
    ssize_t termios_target = console->termios.c_cc[VMIN];
    bool canonical_mode = (console->termios.c_lflag & ICANON);

    spin_lock(&console->lock);
    while (n > 0)
    {
        size_t timeout = console->termios.c_cc[VTIME];  // 1/10s
        timeout = timeout * TIMER_INTERRUPTS_PER_SECOND / 10;
        timeout += kticks_get_ticks();

        // wait until interrupt handler has put some
        // input into console->buffer.
        while (console->r == console->w)
        {
            if (proc_is_killed(get_current()))
            {
                spin_unlock(&console->lock);
                return -ESRCH;
            }
            if (canonical_mode)
            {
                sleep(&console->r, &console->lock);
            }
            else
            {
                size_t now = kticks_get_ticks();
                if (now >= timeout)
                {
                    // timeout expired
                    spin_unlock(&console->lock);
                    return 0;
                }
                // wake up eack kernel tick to check for input
                // if we wait here for a console interrupt, we miss the
                // timeout
                sleep(&g_ticks, &console->lock);
            }
        }

        int32_t c = console->buf[console->r++ % CONSOLE_INPUT_BUF_SIZE];

        if (c == CONTROL_KEY('D'))
        {  // end-of-file
            if (n < target)
            {
                // Save ^D for next time, to make sure
                // caller gets a 0-byte result.
                console->r--;
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
    spin_unlock(&console->lock);

    return target - n;
}

int console_ioctl(struct Device *dev, struct inode *ip, int req, void *ttyctl)
{
    struct Character_Device *cdev = character_device_from_device(dev);
    struct Console_Device *console = console_driver_from_cdev(cdev);

    spin_lock(&console->lock);
    if (req == TCGETA)
    {
        // *termios_p = cons.termios;
        struct termios *termios_out = (struct termios *)ttyctl;

        if (either_copyout(true, (size_t)termios_out,
                           (void *)&(console->termios),
                           sizeof(struct termios)) == -1)
        {
            spin_unlock(&console->lock);
            return -1;
        }
    }
    else if (req == TCSETA)
    {
        // cons.termios = *termios_p;
        struct termios *termios_in = (struct termios *)ttyctl;

        if (either_copyin((void *)&(console->termios), true, (size_t)termios_in,
                          sizeof(struct termios)) == -1)
        {
            spin_unlock(&console->lock);
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
            spin_unlock(&console->lock);
            return -1;
        }
    }
    else
    {
        printk("console_ioctl: unknown request 0x%x\n", req);
        spin_unlock(&console->lock);
        return -1;
    }
    spin_unlock(&console->lock);
    return 0;
}

void console_debug_print_help()
{
    printk("\n");
    printk("CTRL+H: Print this help\n");
    printk("CTRL+N: Print inodes\n");
    printk("CTRL+D: Print dentry cache\n");
    printk("CTRL+P: Print process list\n");
    printk("CTRL+L: Print process list\n");
    printk("CTRL+T: Print process list with page tables\n");
    printk("CTRL+Z: Print kernel memory map\n");
    printk("CTRL+B: Print kernel page table (warning, long!)\n");
    printk("CTRL+U: Print process list with user call stack\n");
    printk("CTRL+S: Print process list with kernel call stack\n");
    printk("CTRL+O: Print process list with open files\n");
    printk("CTRL+Y: Print sys tree\n");
    printk("Time: %zd ticks\n", kticks_get_ticks());
}

void print_epochs()
{
    for (size_t i = 0; i < MAX_CPUS; i++)
    {
        if (g_cpus[i].state == CPU_UNUSED) continue;

        printk("CPU %zd: kernel page table epoch seen: %zu\n", i,
               g_cpus[i].kernel_pgtable_epoch_seen);
    }
}

bool console_handle_control_keys(struct Console_Device *console, int32_t c)
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
            printk("Kernel page table:\n");
            debug_vm_print_page_table(g_kernel_pagetable);
            break;
        case CONTROL_KEY('Z'):
            debug_print_memory_map(&g_kernel_pagetable->memory_map);
            print_epochs();
            break;
        case CONTROL_KEY('Y'): debug_print_kobject_tree(); break;
        case CONTROL_KEY('D'): debug_print_dentry_cache(); break;
        case DELETE_KEY:  // Delete key
            if (console->e != console->w)
            {
                console->e--;
                if (console->termios.c_lflag & ECHO)
                {
                    console_putc(console, BACKSPACE);
                }
            }
            break;
        default: processed = false; break;
    }

    return processed;
}

/// the console input interrupt handler.
/// uart_interrupt_handler() calls this for input character.
/// do erase/kill processing, append to g_console->buf,
/// wake up console_read() if a whole line has arrived.
void console_interrupt_handler(int32_t c)
{
    struct Console_Device *console = g_boot_console;

    spin_lock(&console->lock);

    bool input_processed = false;
    if (console->termios.c_lflag & ICANON)
    {
        input_processed = console_handle_control_keys(console, c);
    }

    if (!input_processed)
    {
        if (c != 0 && console->e - console->r < CONSOLE_INPUT_BUF_SIZE)
        {
            // carriage return to newline
            if (console->termios.c_lflag & ICRNL)
            {
                c = (c == '\r') ? '\n' : c;
            }

            // echo back to the user.
            if (console->termios.c_lflag & ECHO)
            {
                console_putc(console, c);
            }

            // store for consumption by console_read().
            console->buf[console->e++ % CONSOLE_INPUT_BUF_SIZE] = c;

            // in non-canonical mode, return each key press, otherwise wait for
            // newline
            bool wakeup_readers = !(console->termios.c_lflag & ICANON);
            wakeup_readers |= (c == '\n');
            wakeup_readers |= (c == CONTROL_KEY('D'));
            // buffer full:
            wakeup_readers |=
                (console->e - console->r == CONSOLE_INPUT_BUF_SIZE);

            if (wakeup_readers)
            {
                console->w = console->e;
                wakeup(&console->r);
            }
        }
    }

    spin_unlock(&console->lock);
}

struct Console_Device *console_init2(struct TTY_Device *tty)
{
    DEBUG_EXTRA_PANIC(tty != NULL, "tty is NULL");

    struct Console_Device *console =
        kmalloc(sizeof(struct Console_Device), ALLOC_FLAG_ZERO_MEMORY);
    if (console == NULL)
    {
        return NULL;
    }
    if (g_boot_console == NULL)
    {
        g_boot_console = console;
    }

    spin_lock_init(&console->lock, "cons");

    size_t minor = (size_t)atomic_fetch_add(&g_console_next_minor, 1);

    const size_t NAME_LEN = 16;
    char *device_name = kmalloc(NAME_LEN, ALLOC_FLAG_NONE);
    if (device_name == NULL)
    {
        kfree(console);
        printk("console: out of memory\n");
        return NULL;
    }
    snprintf(device_name, NAME_LEN, "console%zd", minor);

    // init device and register it in the system
    dev_init(&console->cdev.dev, CHAR, MKDEV(CONSOLE_DEVICE_MAJOR, minor),
             "console", INVALID_IRQ_NUMBER, NULL);
    console->cdev.ops.read = console_read;
    console->cdev.ops.write = console_write;
    console->cdev.ops.ioctl = console_ioctl;
    console->cdev.dev.mode = 0666;

    memset(&console->termios, 0, sizeof(struct termios));
    console->termios.c_lflag = ECHO | ICANON | ICRNL;
    console->termios.c_cc[VMIN] = 1;   // read() blocks for at least one byte
    console->termios.c_cc[VTIME] = 0;  // no timeout in read()

    console->add_cr = true;

    console->putc = tty->putc;
    console->putc_sync = tty->putc_sync;
    g_console_poll_callback = tty->poll_callback;

    register_device(&console->cdev.dev);
    printk_redirect_to_console(console);

    return console;
}

void console_putc_noop(int32_t ch) {}

dev_t console_init(struct Found_Device *console_dev)
{
    if (console_dev != NULL)
    {
        struct Devices_List *dev_list = get_devices_list();
        dev_t uart_dev = init_device(dev_list, console_dev);
        if (uart_dev == INVALID_DEVICE)
        {
            return INVALID_DEVICE;
        }
    }

    struct Console_Device *console =
        kmalloc(sizeof(struct Console_Device), ALLOC_FLAG_ZERO_MEMORY);
    if (console == NULL)
    {
        return INVALID_DEVICE;
    }
    if (g_boot_console == NULL)
    {
        g_boot_console = console;
    }

    spin_lock_init(&console->lock, "cons");

    size_t minor = (size_t)atomic_fetch_add(&g_console_next_minor, 1);

    // init device and register it in the system
    dev_init(&console->cdev.dev, CHAR, MKDEV(CONSOLE_DEVICE_MAJOR, minor),
             "console", INVALID_IRQ_NUMBER, NULL);
    console->cdev.ops.read = console_read;
    console->cdev.ops.write = console_write;
    console->cdev.ops.ioctl = console_ioctl;
    console->cdev.dev.mode = 0666;

    memset(&console->termios, 0, sizeof(struct termios));
    console->termios.c_lflag = ECHO | ICANON | ICRNL;
    console->termios.c_cc[VMIN] = 1;   // read() blocks for at least one byte
    console->termios.c_cc[VTIME] = 0;  // no timeout in read()

    console->add_cr = true;

    if (console_dev != NULL)
    {
        const char *name = console_dev->driver->dtb_name;
        if (strcmp(name, "ucb,htif0") == 0)
        {
            console->putc = htif_putc;
            console->putc_sync = htif_putc;
            g_console_poll_callback = htif_console_poll_input;
        }
        else if (strcmp(name, "ns16550a") == 0 ||
                 strcmp(name, "snps,dw-apb-uart") == 0)
        {
            // ns16550a or snps,dw-apb-uart
            console->putc = uart_putc;
            console->putc_sync = uart_putc_sync;

            dev_set_irq(&console->cdev.dev,
                        console_dev->init_parameters.interrupt,
                        uart_interrupt_handler);
        }
#if defined(__ARCH_arm64)
        else if (strcmp(name, "brcm,bcm2835-aux-uart") == 0)
        {
            console->putc = bcm2835_aux_uart_putc;
            console->putc_sync = bcm2835_aux_uart_putc_sync;
            g_console_poll_callback = bcm2835_aux_uart_poll_input;

            dev_set_irq(&console->cdev.dev,
                        console_dev->init_parameters.interrupt,
                        bcm2835_aux_uart_interrupt_handler);
        }
        else if (strcmp(name, "arm,pl011") == 0)
        {
            console->putc = arm_pl011_putc;
            console->putc_sync = arm_pl011_putc;
            g_console_poll_callback = arm_pl011_poll_input;

            dev_set_irq(&console->cdev.dev,
                        console_dev->init_parameters.interrupt,
                        arm_pl011_interrupt_handler);
        }
#endif
    }
    else
    {
#ifdef __ARCH_riscv
        if (sbi_probe_extension(SBI_LEGACY_EXT_CONSOLE_PUTCHAR) > 0)
        {
            // SBI console fallback
            console->putc = sbi_console_putchar;
            console->putc_sync = sbi_console_putchar;
            g_console_poll_callback = sbi_console_poll_input;
            printk("Console fallback: SBI\n");
        }
#endif
    }

    if (console->putc == NULL)
    {
        // run with no input/output:
        console->putc = console_putc_noop;
        console->putc_sync = console_putc_noop;
    }

    register_device(&console->cdev.dev);
    printk_redirect_to_console(console);

    return console->cdev.dev.device_number;
}
