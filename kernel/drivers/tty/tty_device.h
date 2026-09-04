/* SPDX-License-Identifier: MIT */
#pragma once

#include <drivers/device.h>
#include <kernel/container_of.h>
#include <kernel/kernel.h>
#include <kernel/list.h>

struct TTY_Device;

// pointers for the put char function
typedef void (*device_putc_fn)(struct TTY_Device *tty, int32_t ch);

typedef void (*device_poll_callback)(struct TTY_Device *tty);

struct Console_Device;

struct TTY_Device
{
    struct Device dev;

    device_putc_fn putc;
    device_putc_fn putc_sync;
    device_poll_callback poll_callback;
    struct Console_Device *console;
};

#define tty_device_from_device(ptr) container_of(ptr, struct TTY_Device, dev)

struct TTY_Callback
{
    device_poll_callback callback;
    struct TTY_Device *payload;
    struct TTY_Callback *next;
};

extern struct TTY_Callback *g_tty_callbacks;
