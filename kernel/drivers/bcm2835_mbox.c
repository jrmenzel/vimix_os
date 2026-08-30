/* SPDX-License-Identifier: MIT */

#include <drivers/bcm2835_mbox.h>
#include <drivers/mmio_access.h>
#include <kernel/major.h>
#include <kernel/spinlock.h>

REGISTER_DRIVER("brcm,bcm2835-mbox", bcm2835_mbox_init);

#define MBOX_READ_OFFSET 0x00
#define MBOX_STATUS_READ_OFFSET 0x18
#define MBOX_WRITE_OFFSET 0x20
#define MBOX_STATUS_WRITE_OFFSET 0x38

#define MBOX_STATUS_FULL 0x80000000u
#define MBOX_STATUS_EMPTY 0x40000000u

#define MBOX_MAX_POLL_ITERATIONS 10000000u

struct bcm2835_mbox
{
    struct spinlock lock;
    size_t mmio_base;  ///< memory map start
    bool is_initialized;
};

struct bcm2835_mbox g_bcm2835_mbox;

dev_t bcm2835_mbox_init(struct Device_Init_Parameters *init_param,
                        const char *name)
{
    spin_lock_init(&g_bcm2835_mbox.lock, "bcm2835_mbox_lock");
    g_bcm2835_mbox.mmio_base = init_param->mem[0].start_va;
    g_bcm2835_mbox.is_initialized = true;

    return MKDEV(BCM2835_MBOX_MAJOR, 0);
}

bool bcm2835_mbox_call(uint8_t channel, uint32_t data, uint32_t *response)
{
    if (!g_bcm2835_mbox.is_initialized || channel > 0xF || (data & 0xF) != 0)
    {
        return false;
    }

    uint32_t msg = data | channel;

    spin_lock(&g_bcm2835_mbox.lock);

    // Wait for room in TX FIFO.
    size_t timeout = MBOX_MAX_POLL_ITERATIONS;
    while (
        (MMIO_READ_UINT_32(g_bcm2835_mbox.mmio_base, MBOX_STATUS_WRITE_OFFSET) &
         MBOX_STATUS_FULL) != 0)
    {
        if (--timeout == 0)
        {
            spin_unlock(&g_bcm2835_mbox.lock);
            return false;
        }
    }

    MMIO_WRITE_UINT_32(g_bcm2835_mbox.mmio_base, MBOX_WRITE_OFFSET, msg);

    // Wait for matching response on same channel.
    timeout = MBOX_MAX_POLL_ITERATIONS;
    while (timeout-- > 0)
    {
        if ((MMIO_READ_UINT_32(g_bcm2835_mbox.mmio_base,
                               MBOX_STATUS_READ_OFFSET) &
             MBOX_STATUS_EMPTY) != 0)
        {
            continue;
        }

        uint32_t resp =
            MMIO_READ_UINT_32(g_bcm2835_mbox.mmio_base, MBOX_READ_OFFSET);
        if ((resp & 0xF) != channel)
        {
            continue;
        }

        if (response != NULL)
        {
            *response = resp;
        }

        spin_unlock(&g_bcm2835_mbox.lock);
        return true;
    }

    spin_unlock(&g_bcm2835_mbox.lock);
    return false;
}
