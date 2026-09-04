/* SPDX-License-Identifier: MIT */
#pragma once

#include <drivers/tty/tty_device.h>
#include <kernel/kernel.h>
#include <kernel/stdatomic.h>

extern atomic_size_t g_ticks;

/// @brief Init the timer g_ticks which will increase
/// with each timer interrupt.
void kticks_init();

/// @brief increase timer by one tick
void kticks_inc_ticks();

/// @brief Get current timer value in ticks since boot
static inline size_t kticks_get_ticks() { return atomic_load(&g_ticks); }

size_t seconds_since_boot();
size_t msec_since_boot();

bool kticks_register_tty_callback(tty_poll_callback callback,
                                  struct TTY_Device *payload);
