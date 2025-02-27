/* SPDX-License-Identifier: MIT */
#pragma once

#include <drivers/devices_list.h>
#include <kernel/kernel.h>

/// @brief
/// @param init_parameters
/// @param name Device name from the dtb file (if one driver supports multiple
/// devices)
/// @return Device number of successful init.
dev_t clint_init(struct Device_Init_Parameters *init_parameters,
                 const char *name);

#ifdef __BOOT_M_MODE
/// arrange to receive timer interrupts.
/// they will arrive in machine mode
/// at m_mode_trap_vector in m_mode_trap_vector.S,
/// which turns them into software interrupts for
/// interrupt_handler() in trap.c.
void clint_init_timer_interrupt();
#endif  // __BOOT_M_MODE
