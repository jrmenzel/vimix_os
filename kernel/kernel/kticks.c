/* SPDX-License-Identifier: MIT */

#include <drivers/tty/console.h>
#include <drivers/tty/tty_device.h>
#include <kernel/kticks.h>
#include <kernel/proc.h>
#include <kernel/spinlock.h>
#include <kernel/stdatomic.h>
#include <kernel/timer.h>

// each call to the timer interrupt is one tick
atomic_size_t g_ticks = 0;

/// @brief boot time from rv_get_time()
uint64_t g_boot_time = 0;

struct TTY_Callback *g_tty_callbacks = NULL;

void kticks_init() { atomic_init(&g_ticks, 0); }

bool kticks_register_tty_callback(tty_poll_callback callback,
                                  struct TTY_Device *payload)
{
    struct TTY_Callback *new_entry =
        kmalloc(sizeof(struct TTY_Callback), ALLOC_FLAG_ZERO_MEMORY);
    if (new_entry == NULL) return false;

    new_entry->callback = callback;
    new_entry->payload = payload;
    new_entry->next = g_tty_callbacks;
    g_tty_callbacks = new_entry;

    return true;
}

void kticks_inc_ticks()
{
    atomic_fetch_add(&g_ticks, 1);

    // The htif and SBI consoles can be a fallback for UART,
    // but without IRQs we need to poll the input manually
    for (struct TTY_Callback *tty_callback = g_tty_callbacks;
         tty_callback != NULL; tty_callback = tty_callback->next)
    {
        tty_callback->callback(tty_callback->payload);
    }
}

size_t seconds_since_boot()
{
    uint64_t now = get_time();
    uint64_t delta = now - g_boot_time;
    return delta / g_timebase_frequency;
}

size_t msec_since_boot()
{
    uint64_t now = get_time();
    uint64_t delta = now - g_boot_time;
    return delta / (g_timebase_frequency / 1000);
}
