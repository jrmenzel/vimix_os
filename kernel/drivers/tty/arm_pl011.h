/* SPDX-License-Identifier: MIT */
#pragma once

#include <drivers/devices_list.h>
#include <kernel/kernel.h>
#include <kernel/spinlock.h>

// ARM PL011 UART driver, found for example on the qemu virt board

dev_t arm_pl011_init(struct Device_Init_Parameters *init_param,
                     const char *name);

void arm_pl011_interrupt_handler(dev_t dev);

void arm_pl011_putc(int32_t c);

int arm_pl011_getc();

void arm_pl011_poll_input();

/// @brief A UART found for example on the qemu virt board
struct arm_pl011
{
    struct spinlock arm_pl011_lock;
    size_t uart_base;
};
