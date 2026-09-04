/* SPDX-License-Identifier: MIT */
#pragma once

#include <drivers/cdev/character_device.h>
#include <drivers/devices_list.h>
#include <drivers/tty/tty_device.h>

struct Console_Device;

/// init console, called from TTY/UART hardware
struct Console_Device *console_init(struct TTY_Device *tty);

/// @brief Called by the interrupt of a TTY when new input is available
/// @param c input key
void console_interrupt_handler(struct Console_Device *console, int32_t c);

/// writes a character to the console, called by printk()
void console_putc(struct Console_Device *console, int32_t c);
