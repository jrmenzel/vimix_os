/* SPDX-License-Identifier: MIT */

//
// Console input and output, to the uart.
// Reads are line at a time.
//

#include <drivers/tty/console.h>
#include <drivers/tty/debug_console.h>
#include <kernel/container_of.h>
#include <kernel/errno.h>
#include <kernel/ioctl.h>
#include <kernel/kticks.h>
#include <kernel/proc.h>
#include <kernel/spinlock.h>
#include <kernel/string.h>
#include <kernel/termios.h>
#include <kernel/timer.h>
#include <mm/kalloc.h>

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

    struct TTY_Device *tty;
    struct Debug_Console *dbg_con;
};

#define console_driver_from_cdev(ptr) \
    container_of((ptr), struct Console_Device, cdev)

atomic_size_t g_console_next_minor = 0;

/// send one character to the uart.
/// called by printk(), and to echo input characters,
/// but not from write().
void console_putc(struct Console_Device *console, int32_t c)
{
    if (c == BACKSPACE)
    {
        // if the user typed backspace, overwrite with a space.
        console->tty->putc_sync(console->tty, '\b');
        console->tty->putc_sync(console->tty, ' ');
        console->tty->putc_sync(console->tty, '\b');
    }
    else
    {
        if (console->add_cr && c == '\n')
        {
            // add carrige return to a newline
            console->tty->putc_sync(console->tty, '\r');
        }
        console->tty->putc_sync(console->tty, c);
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
            console->tty->putc(console->tty, '\r');
        }
        console->tty->putc(console->tty, c);
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

    // if a debug console is active wait
    while (console->dbg_con->is_active)
    {
        sleep(&g_ticks, &console->lock);
    }

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

bool console_handle_control_keys(struct Console_Device *console, int32_t c)
{
    bool processed = true;

    switch (c)
    {
        case CONTROL_KEY('H'): dbg_con_activate(console->dbg_con); break;
        case DELETE_KEY:
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
void console_interrupt_handler(struct Console_Device *console, int32_t c)
{
    spin_lock(&console->lock);

    // if a debug console is active rout input there and skip normal processing
    if (console->dbg_con->is_active)
    {
        dbg_con_handle_input(console->dbg_con, c);
        spin_unlock(&console->lock);
        return;
    }

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

struct Console_Device *console_init(struct TTY_Device *tty)
{
    DEBUG_EXTRA_PANIC(tty != NULL, "tty is NULL");

    struct Console_Device *console =
        kmalloc(sizeof(struct Console_Device), ALLOC_FLAG_ZERO_MEMORY);
    if (console == NULL)
    {
        return NULL;
    }
    console->dbg_con = alloc_init_debug_console(console);
    if (console->dbg_con == NULL)
    {
        kfree(console);
        return NULL;
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
    if (minor == 0)
    {
        // call the first just console
        strncpy(device_name, "console", NAME_LEN);
    }
    else
    {
        snprintf(device_name, NAME_LEN, "console%zd", minor);
    }

    // init device and register it in the system
    dev_init(&console->cdev.dev, CHAR, MKDEV(CONSOLE_DEVICE_MAJOR, minor),
             device_name, NULL, 0, NULL);
    console->cdev.ops.read = console_read;
    console->cdev.ops.write = console_write;
    console->cdev.ops.ioctl = console_ioctl;
    console->cdev.dev.mode = 0666;

    memset(&console->termios, 0, sizeof(struct termios));
    console->termios.c_lflag = ECHO | ICANON | ICRNL;
    console->termios.c_cc[VMIN] = 1;   // read() blocks for at least one byte
    console->termios.c_cc[VTIME] = 0;  // no timeout in read()

    console->add_cr = true;

    console->tty = tty;
    if (tty->poll_callback)
    {
        bool ok = kticks_register_tty_callback(tty->poll_callback, tty);
        if (!ok)
        {
            printk("Error registering regular callback for TTY\n");
        }
    }

    register_device(&console->cdev.dev);

    if (printk_has_console() == false)
    {
        printk_set_console(console);
    }

    return console;
}
