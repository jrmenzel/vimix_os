/* SPDX-License-Identifier: MIT */

#include <drivers/devices_list.h>
#include <drivers/tty/tty_device.h>
#include <kernel/kernel.h>
#include <kernel/spinlock.h>

// used on some emulators like Spike for basic IO and system shutdown.
struct HTIF
{
    struct TTY_Device tty;

    volatile uint64_t *tohost;
    volatile uint64_t *fromhost;

    // HTIF has a single command mailbox shared by all CPUs. Console reads are
    // asynchronous: Spike only replies after input becomes available.
    struct spinlock lock;
    bool console_read_pending;
};

#define htif_from_tty(ptr) container_of(ptr, struct HTIF, tty)

/// @brief Init function
/// @param init_parameters Only the memory address is relevant.
/// @param name Device name from the dtb file (if one driver supports multiple
/// devices)
/// @return Device number of successful init.
dev_t htif_init(struct Device_Init_Parameters *init_parameters,
                const char *name);
