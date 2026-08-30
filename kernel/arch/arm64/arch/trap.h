/* SPDX-License-Identifier: MIT */
#pragma once

#include <kernel/cpu.h>
#include <kernel/kernel.h>
#include <kernel/spinlock.h>

/// @brief Set kernel mode trap vector
void set_supervisor_trap_vector();
