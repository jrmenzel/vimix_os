/* SPDX-License-Identifier: MIT */
#pragma once

#include <kernel/kernel.h>

/// @brief Reboot or panic
void machine_restart();

/// @brief Halt machine or panic
void machine_power_off();
