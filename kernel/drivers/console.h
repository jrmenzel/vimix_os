/* SPDX-License-Identifier: MIT */
#pragma once

#include <drivers/devices_list.h>
#include <kernel/types.h>

/// init console and console hardware (e.g. UART)
dev_t console_init(struct Device_Memory_Map *dev_map);

/// @brief Called by the interrupt when new input is available
/// @param c input key
void console_interrupt_handler(int32_t c);

/// writes a character to the console
void console_putc(int32_t c);
