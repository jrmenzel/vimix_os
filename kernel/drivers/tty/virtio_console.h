/* SPDX-License-Identifier: MIT */
#pragma once

#include <drivers/misc/virtio.h>
#include <drivers/tty/tty_device.h>
#include <kernel/spinlock.h>

#define VIRTIO_CONSOLE_RX_QUEUE 0
#define VIRTIO_CONSOLE_TX_QUEUE 1
#define VIRTIO_CONSOLE_RX_BUFFER_SIZE 64

struct Virtio_Console
{
    struct TTY_Device tty;
    struct Virtio_Device virtio;
    struct Virtio_Queue receiveq;
    struct Virtio_Queue transmitq;
    struct spinlock tx_lock;
    char rx_buffer[VIRTIO_DESCRIPTORS][VIRTIO_CONSOLE_RX_BUFFER_SIZE];
    char tx_buffer[VIRTIO_DESCRIPTORS];
};

#define virtio_console_from_tty(ptr) \
    container_of(ptr, struct Virtio_Console, tty)

dev_t virtio_console_init(struct Device_Init_Parameters *init_parameters,
                          const char *name);
