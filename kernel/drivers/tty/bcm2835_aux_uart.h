/* SPDX-License-Identifier: MIT */
#pragma once

#include <drivers/devices_list.h>
#include <drivers/tty/tty_device.h>
#include <kernel/spinlock.h>

dev_t bcm2835_aux_uart_init(struct Device_Init_Parameters *init_param,
                            const char *name);

void bcm2835_aux_uart_interrupt_handler(dev_t dev);

void bcm2835_aux_uart_poll_input();

void bcm2835_aux_uart_putc(int32_t c);

void bcm2835_aux_uart_putc_sync(int32_t c);

int bcm2835_aux_uart_getc();

/// @brief A UART found for example on the Raspberry Pi 4
struct bcm2835_aux_uart
{
    struct TTY_Device tty;

    struct spinlock lock;
    size_t mmio_base;  ///< memory map start
    size_t clock_hz;
};
