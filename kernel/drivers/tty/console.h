/* SPDX-License-Identifier: MIT */
#pragma once

#include <drivers/cdev/character_device.h>
#include <drivers/devices_list.h>
#include <drivers/tty/tty_device.h>

extern void (*g_console_poll_callback)();

struct Console_Device;

/// init console and console hardware (e.g. UART)
dev_t console_init(struct Found_Device *console_dev);

struct Console_Device *console_init2(struct TTY_Device *tty);

/// @brief Called by the interrupt when new input is available
/// @param c input key
void console_interrupt_handler(int32_t c);

/// writes a character to the console
void console_putc(struct Console_Device *console, int32_t c);
