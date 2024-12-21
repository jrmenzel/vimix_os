/* SPDX-License-Identifier: MIT */
#pragma once

#include <kernel/kernel.h>

extern void (*g_machine_restart_func)();
extern void (*g_machine_power_off_func)();

/// @brief Reboot or panic
void machine_restart();

/// @brief Halt machine or panic
void machine_power_off();
