/* SPDX-License-Identifier: MIT */
#pragma once

#include <kernel/kernel.h>

// pointers for the put char function
typedef void (*device_putc_fn)(int32_t ch);

typedef void (*device_poll_callback)();

struct TTY_Device
{
    device_putc_fn putc;
    device_putc_fn putc_sync;
    device_poll_callback poll_callback;
};
