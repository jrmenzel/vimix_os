/* SPDX-License-Identifier: MIT */
#pragma once

#include <kernel/kernel.h>

extern uint32_t g_ticks;
void trap_init();
void trap_init_per_cpu();
extern struct spinlock g_tickslock;
void return_to_user_mode();
