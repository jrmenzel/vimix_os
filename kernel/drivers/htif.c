/* SPDX-License-Identifier: MIT */

#include <drivers/console.h>
#include <drivers/htif.h>
#include <kernel/major.h>
#include <kernel/reset.h>

bool htif_is_initialized = false;

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

volatile uint64_t tohost = 0;
volatile uint64_t fromhost = 0;

volatile uint64_t *htif_tohost = NULL;
volatile uint64_t *htif_fromhost = NULL;

uint64_t htif_send_command(uint32_t device, uint32_t command, uint64_t data)
{
    // upper 8 bit device
    uint64_t request = 0;
#if defined(__ARCH_is_32bit)
    // on 32 bit the upper 32 bits of the tohost/fromhost
    // registers are set to 0. So only the halt device with the halt command
    // are supported.
    data = data & 0xFFFFFFFF;
    request = data;
#else
    device = device & 0x0F;
    request = (uint64_t)device << 56;

    // next 8 bit the command
    uint64_t command64 = command & 0x0F;
    command64 = command64 << 48;
    request = request | command64;

    // remaining bits are data
    data = data & 0xFFFFFFFFFFFFFF;
    request = request | data;
#endif

    // Communication protocol:
    // 1. write 0 to HTIF_REGISTER_FROMHOST
    // 2. write the request to HTIF_REGISTER_TOHOST
    // 3. read response from HTIF_REGISTER_FROMHOST
    *htif_fromhost = 0;
    *htif_tohost = request;
    return *htif_fromhost;
}

void htif_machine_power_off()
{
    htif_send_command(HTIF_DEVICE_HALT, HTIF_HALT_HALT, 1);
    while (true)
    {
    }  // at least on Spike the shutdown is not instant, prevent a panic
}

void htif_putc(int32_t c)
{
    htif_send_command(HTIF_DEVICE_CONSOLE, HTIF_CONSOLE_PUTCHAR, c);
}

ssize_t htif_getc()
{
    // 0 if nothing to read, char+1 otherwise -> -1 for char and -1 on error
    uint64_t v =
        htif_send_command(HTIF_DEVICE_CONSOLE, HTIF_CONSOLE_GETCHAR, 0);
    return (ssize_t)v - 1;
}

void htif_console_poll_input()
{
    // read and process incoming characters.
    while (true)
    {
        ssize_t c = htif_getc();
        if (c == -1)
        {
            break;
        }
        console_interrupt_handler(c);
    }
}

dev_t htif_init(struct Device_Init_Parameters *init_parameters,
                const char *name)
{
    if (htif_is_initialized)
    {
        // can happen as htif might get initialized as a boot console and later
        // hoping to get reboot/halt functions
        return MKDEV(HTIF_MAJOR, 0);
    }

    if (init_parameters->mem[0].start == 0)
    {
        // kernel defined tohost/fromhost
        htif_tohost = &tohost;
        htif_fromhost = &fromhost;
    }
    else
    {
        htif_tohost = (volatile uint64_t *)(init_parameters->mem[0].start +
                                            HTIF_REGISTER_TOHOST);
        htif_fromhost = (volatile uint64_t *)(init_parameters->mem[0].start +
                                              HTIF_REGISTER_FROMHOST);
    }
    printk("register HTIF shutdown function\n");
    g_machine_power_off_func = &htif_machine_power_off;

    htif_is_initialized = true;
    return MKDEV(HTIF_MAJOR, 0);
}
