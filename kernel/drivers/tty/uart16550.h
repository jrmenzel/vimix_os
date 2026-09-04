/* SPDX-License-Identifier: MIT */
#pragma once

#include <drivers/devices_list.h>
#include <drivers/tty/tty_device.h>
#include <kernel/kernel.h>
#include <kernel/spinlock.h>

#define UART_TX_BUF_SIZE 32

/// @brief Struct of the driver for the common 16550 UART.
struct uart_16550
{
    struct TTY_Device tty;

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
    size_t mmio_base;

    int32_t reg_io_width;

    /// @brief Register addresses are shifted by this amount of bits to allow
    /// different register width
    int32_t reg_shift;
};

#define uart_16550_from_tty(ptr) container_of(ptr, struct uart_16550, tty)

enum UART_BAUD_RATE
{
    BAUD_1200,
    BAUD_2400,
    BAUD_4800,
    BAUD_9600,
    BAUD_19200,
    BAUD_38400,
    BAUD_57600,
    BAUD_115200
};

/// @brief Inits the hardware, creates a uart_16550 object
/// and adds it to the devices list.
dev_t uart_init(struct Device_Init_Parameters *init_parameters,
                const char *name);

bool uart_set_baud_rate(struct uart_16550 *uart, enum UART_BAUD_RATE rate);

void uart_send_buffer(struct uart_16550 *uart);

void uart_putc(struct TTY_Device *tty, int32_t c);
void uart_putc_sync(struct TTY_Device *tty, int32_t c);
void uart_interrupt_handler(dev_t dev);
