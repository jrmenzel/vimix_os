/* SPDX-License-Identifier: MIT */

#include <arch/asm.h>
#include <drivers/devices_list.h>
#include <drivers/driver.h>
#include <drivers/tty/console.h>
#include <drivers/tty/virtio_console.h>
#include <kernel/major.h>
#include <kernel/pgtable.h>
#include <kernel/proc.h>
#include <kernel/stdatomic.h>
#include <mm/kalloc.h>

REGISTER_DRIVER("virtio,mmio", virtio_console_init);

atomic_size_t g_virtio_console_next_minor = 0;

static void virtio_console_interrupt(dev_t dev);
static void virtio_console_putc(struct TTY_Device *tty, int32_t c);
static void virtio_console_putc_sync(struct TTY_Device *tty, int32_t c);

static void virtio_console_fill_receiveq(struct Virtio_Console *console)
{
    for (size_t i = 0; i < VIRTIO_DESCRIPTORS; ++i)
    {
        int32_t descriptor = virtio_queue_alloc_desc(&console->receiveq);
        if (descriptor < 0) panic("virtio console: receive queue full");

        console->receiveq.desc[descriptor].addr =
            virt_to_phys((size_t)console->rx_buffer[descriptor]);
        console->receiveq.desc[descriptor].len = VIRTIO_CONSOLE_RX_BUFFER_SIZE;
        console->receiveq.desc[descriptor].flags = VRING_DESC_F_WRITE;
        virtio_queue_submit(&console->receiveq, descriptor);
    }
}

static void virtio_console_destroy(struct Virtio_Console *console)
{
    virtio_queue_destroy(&console->receiveq);
    virtio_queue_destroy(&console->transmitq);
    kfree(console);
}

dev_t virtio_console_init(struct Device_Init_Parameters *init_parameters,
                          const char *name)
{
    DRIVER_CHECK_INIT_PARAMS(init_parameters);
    if (!virtio_mmio_is_device(init_parameters, VIRTIO_DEVICE_ID_CONSOLE))
    {
        // not a virtio console
        return INVALID_DEVICE;
    }

    struct Virtio_Console *console =
        kmalloc(sizeof(struct Virtio_Console), ALLOC_FLAG_ZERO_MEMORY);
    if (console == NULL)
    {
        return INVALID_DEVICE;
    }

    if (!virtio_device_begin(&console->virtio, init_parameters,
                             VIRTIO_DEVICE_ID_CONSOLE, 0) ||
        !virtio_queue_init(&console->virtio, &console->receiveq,
                           VIRTIO_CONSOLE_RX_QUEUE) ||
        !virtio_queue_init(&console->virtio, &console->transmitq,
                           VIRTIO_CONSOLE_TX_QUEUE))
    {
        virtio_device_fail(&console->virtio);
        virtio_console_destroy(console);
        return INVALID_DEVICE;
    }

    spin_lock_init(&console->tx_lock, "virtio_console_tx");
    syserr_t err = dev_init(
        &console->tty.dev, OTHER, VIRTIO_CONSOLE_MAJOR,
        &g_virtio_console_next_minor, "virtcon", init_parameters->interrupts,
        init_parameters->interrupt_count, virtio_console_interrupt, NULL);
    if (err != 0)
    {
        virtio_device_fail(&console->virtio);
        virtio_console_destroy(console);
        return INVALID_DEVICE;
    }

    console->tty.putc = virtio_console_putc;
    console->tty.putc_sync = virtio_console_putc_sync;
    console->tty.poll_callback = NULL;
    console->tty.set_baud_rate = tty_set_baud_rate_unsupported;

    virtio_console_fill_receiveq(console);
    virtio_device_finish(&console->virtio);
    virtio_queue_notify(&console->receiveq);

    console->tty.console = console_init(&console->tty);
    if (!console->tty.console)
    {
        virtio_device_fail(&console->virtio);
        kfree((void *)console->tty.dev.name);
        virtio_console_destroy(console);
        return INVALID_DEVICE;
    }

    register_device(&console->tty.dev);
    return console->tty.dev.device_number;
}

static void virtio_console_putc(struct TTY_Device *tty, int32_t c)
{
    struct Virtio_Console *console = virtio_console_from_tty(tty);

    spin_lock(&console->tx_lock);
    int32_t descriptor;
    while ((descriptor = virtio_queue_alloc_desc(&console->transmitq)) < 0)
        sleep(&console->transmitq.free[0], &console->tx_lock);

    console->tx_buffer[descriptor] = (char)c;
    console->transmitq.desc[descriptor].addr =
        virt_to_phys((size_t)&console->tx_buffer[descriptor]);
    console->transmitq.desc[descriptor].len = 1;
    console->transmitq.desc[descriptor].flags = 0;
    virtio_queue_submit(&console->transmitq, descriptor);
    spin_unlock(&console->tx_lock);
}

// Used by printk and other contexts which require the character to have left
// the queue before returning. Holding tx_lock prevents the interrupt handler
// from consuming this descriptor while it is polled here.
static void virtio_console_putc_sync(struct TTY_Device *tty, int32_t c)
{
    struct Virtio_Console *console = virtio_console_from_tty(tty);

    spin_lock(&console->tx_lock);
    int32_t descriptor;
    while ((descriptor = virtio_queue_alloc_desc(&console->transmitq)) < 0)
    {
        struct virtq_used_elem completed;
        if (virtio_queue_pop_used(&console->transmitq, &completed))
        {
            if (completed.id >= VIRTIO_DESCRIPTORS)
                panic("virtio console: invalid transmit descriptor");
            virtio_queue_free_desc(&console->transmitq, completed.id);
        }
        else
        {
            ARCH_ASM_NOP;
        }
    }

    console->tx_buffer[descriptor] = (char)c;
    console->transmitq.desc[descriptor].addr =
        virt_to_phys((size_t)&console->tx_buffer[descriptor]);
    console->transmitq.desc[descriptor].len = 1;
    console->transmitq.desc[descriptor].flags = 0;
    virtio_queue_submit(&console->transmitq, descriptor);

    struct virtq_used_elem used;
    while (true)
    {
        if (!virtio_queue_pop_used(&console->transmitq, &used))
        {
            ARCH_ASM_NOP;
            continue;
        }
        if (used.id >= VIRTIO_DESCRIPTORS)
            panic("virtio console: invalid transmit descriptor");
        virtio_queue_free_desc(&console->transmitq, used.id);
        if (used.id == (uint32_t)descriptor) break;
    }
    spin_unlock(&console->tx_lock);
}

static void virtio_console_interrupt(dev_t dev)
{
    struct Device *device = dev_by_device_number(dev);
    struct TTY_Device *tty = tty_device_from_device(device);
    struct Virtio_Console *console = virtio_console_from_tty(tty);

    virtio_device_ack_interrupt(&console->virtio);

    spin_lock(&console->tx_lock);
    struct virtq_used_elem used;
    while (virtio_queue_pop_used(&console->transmitq, &used))
    {
        if (used.id >= VIRTIO_DESCRIPTORS)
            panic("virtio console: invalid transmit descriptor");
        virtio_queue_free_desc(&console->transmitq, used.id);
    }
    spin_unlock(&console->tx_lock);

    while (virtio_queue_pop_used(&console->receiveq, &used))
    {
        if (used.id >= VIRTIO_DESCRIPTORS ||
            used.len > VIRTIO_CONSOLE_RX_BUFFER_SIZE)
            panic("virtio console: invalid receive buffer");

        for (size_t i = 0; i < used.len; ++i)
            console_interrupt_handler(console->tty.console,
                                      console->rx_buffer[used.id][i]);

        console->receiveq.desc[used.id].addr =
            virt_to_phys((size_t)console->rx_buffer[used.id]);
        console->receiveq.desc[used.id].len = VIRTIO_CONSOLE_RX_BUFFER_SIZE;
        console->receiveq.desc[used.id].flags = VRING_DESC_F_WRITE;
        virtio_queue_submit(&console->receiveq, used.id);
    }
}
