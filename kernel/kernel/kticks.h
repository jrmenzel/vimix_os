/* SPDX-License-Identifier: MIT */
#pragma once

#include <kernel/kernel.h>

extern size_t g_ticks;
extern struct spinlock g_tickslock;

/// @brief Init the timer g_ticks which will increase
/// with each timer interrupt.
void kticks_init();

/// @brief increase timer by one tick
void kticks_inc_ticks();

/// @brief Get current timer value in ticks since boot
size_t kticks_get_ticks();

/// @brief Get current timer value in seconds since boot
size_t kticks_get_seconds();
