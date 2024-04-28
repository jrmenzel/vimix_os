/* SPDX-License-Identifier: MIT */
#pragma once

#include <kernel/types.h>

/// @brief Inits the uart_16550 hardware
void uart_init();

/// @brief Console Character Device will set this up
void uart_interrupt_handler();

/// add a character to the output buffer and tell the
/// UART to start sending if it isn't already.
/// blocks if the output buffer is full.
/// because it may block, it can't be called
/// from interrupts; it's only suitable for use
/// by write().
void uart_putc(int32_t c);

/// alternate version of uart_putc() that doesn't
/// use interrupts, for use by kernel printk() and
/// to echo characters. it spins waiting for the uart's
/// output register to be empty.
void uart_putc_sync(int32_t c);

/// read one input character from the UART.
/// return -1 if none is waiting.
int uart_getc();
