/* SPDX-License-Identifier: MIT */
#pragma once

#include <kernel/kernel.h>

extern size_t g_ticks;
extern struct spinlock g_tickslock;

/// @brief Init the timer g_ticks which will increase
/// with each timer interrupt.
void trap_init();

/// @brief Set kernel mode trap vector
void trap_init_per_cpu();

/// @brief called after serving syscalls and after a fork
void return_to_user_mode();
