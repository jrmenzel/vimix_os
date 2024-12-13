/* SPDX-License-Identifier: MIT */
#pragma once

#include <drivers/devices_list.h>
#include <kernel/spinlock.h>
#include <kernel/types.h>

/// @brief Inits the hardware, creates a uart_16550 object
/// and adds it to the devices list.
void uart_init(struct Device_Init_Parameters *init_param);

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

#define UART_TX_BUF_SIZE 32

/// @brief Struct of the driver for the common 16550 UART.
struct uart_16550
{
    struct spinlock uart_tx_lock;

    /// the transmit output buffer.
    char uart_tx_buf[UART_TX_BUF_SIZE];

    /// write index
    /// (next write goes to uart_tx_buf[uart_tx_w % UART_TX_BUF_SIZE])
    size_t uart_tx_w;

    /// read index
    /// (next read from uart_tx_buf[uart_tx_r % UART_TX_BUF_SIZE])
    size_t uart_tx_r;

    /// the UART control registers are memory-mapped to this address
    size_t uart_base;

    int32_t reg_io_width;

    /// @brief Register addresses are shifted by this amount of bits to allow
    /// different register width
    int32_t reg_shift;

    enum UART_Variant
    {
        NS16550,
        DW_APT
    } uart_variant;
};
