/* SPDX-License-Identifier: MIT */
#pragma once

#include <drivers/devices_list.h>
#include <drivers/tty/tty_device.h>
#include <kernel/kernel.h>
#include <kernel/spinlock.h>

// ARM PL011 UART driver, found for example on the qemu virt board

dev_t arm_pl011_init(struct Device_Init_Parameters *init_parameters,
                     const char *name);

/// @brief A UART found for example on the qemu virt board
struct Arm_pl011
{
    struct TTY_Device tty;

    struct spinlock arm_pl011_lock;
    size_t mmio_base;
};

#define arm_pl011_from_tty(ptr) container_of(ptr, struct Arm_pl011, tty)

void arm_pl011_putc(struct TTY_Device *tty, int32_t c);
void arm_pl011_poll_input(struct TTY_Device *tty);
void arm_pl011_interrupt_handler(dev_t dev);
