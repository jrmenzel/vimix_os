/* SPDX-License-Identifier: MIT */
#pragma once

#include <drivers/device.h>
#include <kernel/container_of.h>
#include <kernel/kernel.h>
#include <kernel/list.h>

struct TTY_Device;

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

// @brief pointers for the put char function
typedef void (*tty_putc_fn)(struct TTY_Device *tty, int32_t ch);

/// @brief Optional polling callback. Real devices use interrupts and wont need
/// this.
typedef void (*tty_poll_callback)(struct TTY_Device *tty);

typedef syserr_t (*tty_set_baud_rate)(struct TTY_Device *tty,
                                      enum UART_BAUD_RATE rate);

/// @brief Helper for all TTYs where setting the baud rate is not supported.
/// @param tty ignored
/// @param rate ignored
/// @return Error
syserr_t tty_set_baud_rate_unsupported(struct TTY_Device *tty,
                                       enum UART_BAUD_RATE rate);

/// @brief Helper function to get the integer value from the common baud
/// rate enums.
uint32_t tty_get_baud_value(enum UART_BAUD_RATE baud);

struct Console_Device;

struct TTY_Device
{
    struct Device dev;

    tty_putc_fn putc;
    tty_putc_fn putc_sync;
    tty_poll_callback poll_callback;
    tty_set_baud_rate set_baud_rate;
    struct Console_Device *console;
};

#define tty_device_from_device(ptr) container_of(ptr, struct TTY_Device, dev)

struct TTY_Callback
{
    tty_poll_callback callback;
    struct TTY_Device *payload;
    struct TTY_Callback *next;
};

extern struct TTY_Callback *g_tty_callbacks;
