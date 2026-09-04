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
struct spinlock g_tty_callbacks_lock;

void kticks_init()
{
    atomic_init(&g_ticks, 0);
    spin_lock_init(&g_tty_callbacks_lock, "tty_cb");
}

bool kticks_register_tty_callback(tty_poll_callback callback,
                                  struct TTY_Device *payload)
{
    struct TTY_Callback *new_entry =
        kmalloc(sizeof(struct TTY_Callback), ALLOC_FLAG_ZERO_MEMORY);
    if (new_entry == NULL) return false;

    new_entry->callback = callback;
    new_entry->payload = payload;
    new_entry->next = g_tty_callbacks;
    spin_lock(&g_tty_callbacks_lock);
    g_tty_callbacks = new_entry;
    spin_unlock(&g_tty_callbacks_lock);

    return true;
}

void kticks_inc_ticks()
{
    atomic_fetch_add(&g_ticks, 1);

    // The htif and SBI consoles can be a fallback for UART,
    // but without IRQs we need to poll the input manually
    spin_lock(&g_tty_callbacks_lock);
    struct TTY_Callback *tty_callback = g_tty_callbacks;
    while (tty_callback != NULL)
    {
        tty_callback->callback(tty_callback->payload);
        tty_callback = tty_callback->next;
    }
    spin_unlock(&g_tty_callbacks_lock);
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
