/* SPDX-License-Identifier: MIT */

#include <arch/asm.h>
#include <arch/irq.h>
#include <drivers/driver.h>
#include <drivers/tty/console.h>
#include <drivers/tty/htif.h>
#include <kernel/pgtable.h>
#include <kernel/reset.h>

REGISTER_DRIVER("ucb,htif0", htif_init);

/// HTIF is a simple debug interface to emulators and (rarely) hardware.
/// It's used as a console and to halt the machine / emulator.
/// It contains the following registers:

#define HTIF_REGISTER_TOHOST (0x00)
#define HTIF_REGISTER_FROMHOST (0x08)
#define HTIF_REGISTER_IHALT (0x10)
#define HTIF_REGISTER_ICONSOLE (0x18)
#define HTIF_REGISTER_IYIELD (0x20)

#define HTIF_DEVICE_HALT 0
#define HTIF_DEVICE_CONSOLE 1
#define HTIF_DEVICE_YIELD 2

/// Commands

#define HTIF_HALT_HALT 0

#define HTIF_CONSOLE_GETCHAR 0
#define HTIF_CONSOLE_PUTCHAR 1

/// Instead of providing a memory map, a simulator can require the app
/// to contain the symbols "tohost" and "fromhost" and will use these
/// locations to communicate.
/// This can be found in Spike.

extern volatile uint64_t tohost;
extern volatile uint64_t fromhost;

// there should be only one
struct HTIF *g_htif = NULL;

static void htif_send_command(struct HTIF *htif, uint32_t device,
                              uint32_t command, uint64_t data)
{
    // upper 8 bit device
    uint64_t request = 0;
#if defined(__ARCH_32BIT)
    // on 32 bit the upper 32 bits of the tohost/fromhost
    // registers are set to 0. So only the halt device with the halt command
    // are supported.
    data = data & 0xFFFFFFFF;
    request = data;
#else
    device = device & 0xFF;
    request = (uint64_t)device << 56;

    // next 8 bit the command
    uint64_t command64 = command & 0xFF;
    command64 = command64 << 48;
    request = request | command64;

    // remaining bits are data
    data = data & 0xFFFFFFFFFFFFFF;
    request = request | data;
#endif

    // Do not overwrite a command which Spike has not consumed yet. Not every
    // command has a response (console writes do not), so tohost becoming zero
    // is the command-completion indication on this side of the interface.
    while (*htif->tohost != 0)
    {
        ARCH_ASM_NOP;
    }
    atomic_thread_fence(memory_order_seq_cst);
    *htif->tohost = request;
    atomic_thread_fence(memory_order_seq_cst);
}

void htif_machine_power_off()
{
    spin_lock(&g_htif->lock);
    htif_send_command(g_htif, HTIF_DEVICE_HALT, HTIF_HALT_HALT, 1);

    // at least on Spike the shutdown is not instant, prevent a panic
    infinite_loop;
}

void htif_putc(struct TTY_Device *tty, int32_t c)
{
    struct HTIF *htif = htif_from_tty(tty);

    spin_lock(&htif->lock);
    htif_send_command(htif, HTIF_DEVICE_CONSOLE, HTIF_CONSOLE_PUTCHAR, c);
    spin_unlock(&htif->lock);
}

ssize_t htif_getc(struct HTIF *htif)
{
    ssize_t result = -1;

    spin_lock(&htif->lock);

    uint64_t response = *htif->fromhost;
    if (response != 0)
    {
        *htif->fromhost = 0;
        atomic_thread_fence(memory_order_seq_cst);
        htif->console_read_pending = false;

        uint32_t device = (uint32_t)(response >> 56);
        uint32_t command = (uint32_t)((response >> 48) & 0xFF);
        uint64_t payload = response & 0xFFFFFFFFFFFFull;

        // Spike's console response uses bit 8 as the "character available"
        // flag and stores the character in the low byte.
        if (device == HTIF_DEVICE_CONSOLE && command == HTIF_CONSOLE_GETCHAR &&
            (payload & 0x100) != 0)
        {
            result = (ssize_t)(payload & 0xFF);
        }
    }

    // A read request receives no response until input is available. Keep at
    // most one outstanding request and collect it on a later timer poll.
    if (!htif->console_read_pending)
    {
        htif_send_command(htif, HTIF_DEVICE_CONSOLE, HTIF_CONSOLE_GETCHAR, 0);
        htif->console_read_pending = true;
    }

    spin_unlock(&htif->lock);
    return result;
}

void htif_console_poll_input(struct TTY_Device *tty)
{
    struct HTIF *htif = htif_from_tty(tty);

    // read and process incoming characters.
    while (true)
    {
        ssize_t c = htif_getc(htif);
        if (c == -1)
        {
            break;
        }
        console_interrupt_handler(tty->console, c);
    }
}

dev_t htif_init(struct Device_Init_Parameters *init_parameters,
                const char *name)
{
    // note: no DRIVER_CHECK_INIT_PARAMS() as ->mem[0].start_pa can be 0!
    DEBUG_EXTRA_PANIC(init_parameters != NULL,
                      "Driver init parameters are NULL");

    dev_t dev_id = MKDEV(HTIF_MAJOR, 0);
    if (g_htif != NULL)
    {
        // can happen as htif might get initialized as a boot console and later
        // hoping to get reboot/halt functions
        return dev_id;
    }

    g_htif = kmalloc(sizeof(struct HTIF), ALLOC_FLAG_ZERO_MEMORY);
    if (g_htif == NULL)
    {
        return INVALID_DEVICE;
    }
    spin_lock_init(&g_htif->lock, "htif");

    if (init_parameters->mem[0].start_pa == 0)
    {
        // kernel defined tohost/fromhost
        g_htif->tohost = &tohost;
        g_htif->fromhost = &fromhost;
    }
    else
    {
        size_t htif_mmio_base = init_parameters->mem[0].start_va;
        g_htif->tohost =
            (volatile uint64_t *)(htif_mmio_base + HTIF_REGISTER_TOHOST);
        g_htif->fromhost =
            (volatile uint64_t *)(htif_mmio_base + HTIF_REGISTER_FROMHOST);
    }
    printk("register HTIF shutdown function\n");
    g_machine_power_off_func = &htif_machine_power_off;

    if (printk_has_console() == false)
    {
        // Use HTIF as a fallback console only if no other one was found before.
        // HTIF is only used in emulators and the emulated other TTY likely
        // collides with this by both being wired to one stdin/stdout of the
        // emulator.
        g_htif->tty.putc = htif_putc;
        g_htif->tty.putc_sync = htif_putc;
        g_htif->tty.poll_callback = htif_console_poll_input;

        g_htif->tty.console = console_init(&g_htif->tty);
        if (g_htif->tty.console == NULL)
        {
            kfree(g_htif);
            g_htif = NULL;
            return INVALID_DEVICE;
        }
    }

    // init device and register it in the system
    dev_init(&g_htif->tty.dev, OTHER, dev_id, "htif",
             init_parameters->interrupts, init_parameters->interrupt_count,
             NULL);
    register_device(&g_htif->tty.dev);

    return dev_id;
}
