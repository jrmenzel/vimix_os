/* SPDX-License-Identifier: MIT */

#include <arch/interrupts.h>
#include <drivers/block_device.h>
#include <drivers/character_device.h>
#include <drivers/device.h>
#include <kernel/bio.h>
#include <kernel/kernel.h>
#include <kernel/proc.h>

/// @brief all devices indexed by the major device number * MAX_MINOR_DEVICES
struct Device *g_devices[MAX_DEVICES * MAX_MINOR_DEVICES] = {NULL};

void register_device(struct Device *dev)
{
    size_t major = MAJOR(dev->device_number);
    size_t minor = MINOR(dev->device_number);

    if ((major >= MAX_DEVICES) || (minor >= MAX_MINOR_DEVICES))
    {
        panic("invalid high device number");
    }
    if (g_devices[DEVICE_INDEX(major, minor)] != NULL)
    {
        panic("multiple drivers with same device number");
    }

    // printk("register device %d\n", dev->device_number);
    g_devices[DEVICE_INDEX(major, minor)] = dev;

    // hook up interrupts:
    if (dev->irq_number != INVALID_IRQ_NUMBER)
    {
        interrupt_controller_set_interrupt_priority(dev->irq_number, 1);
        // printk("register device %d with IRQ %d\n", dev->device_number,
        //        dev->irq_number);
    }
}

void dev_set_irq(struct Device *dev, int32_t irq_number,
                 interrupt_handler_p interrupt_handler)
{
    dev->irq_number = irq_number;
    dev->dev_ops.interrupt_handler = interrupt_handler;
}

#ifdef CONFIG_DEBUG_EXTRA_RUNTIME_TESTS
#define DEVICE_IS_OK(major, minor, TYPE)                          \
    if ((major >= MAX_DEVICES) || (minor >= MAX_MINOR_DEVICES) || \
        (g_devices[DEVICE_INDEX(major, minor)] == NULL) ||        \
        (g_devices[DEVICE_INDEX(major, minor)]->type != TYPE))    \
    {                                                             \
        panic("no device of that type");                          \
        return NULL;                                              \
    }
#else
#define DEVICE_IS_OK(major, TYPE)
#endif

struct Block_Device *get_block_device(dev_t device_number)
{
    size_t major = MAJOR(device_number);
    size_t minor = MINOR(device_number);

    DEVICE_IS_OK(major, minor, BLOCK);

    return block_device_from_device(g_devices[DEVICE_INDEX(major, minor)]);
}

struct Character_Device *get_character_device(dev_t device_number)
{
    size_t major = MAJOR(device_number);
    size_t minor = MINOR(device_number);

    DEVICE_IS_OK(major, minor, CHAR);

    return character_device_from_device(g_devices[DEVICE_INDEX(major, minor)]);
}

ssize_t block_device_rw(struct Block_Device *bdev, size_t addr_u, size_t offset,
                        size_t n, bool do_read)
{
    if (offset >= bdev->size) return 0;

    if (offset + n > bdev->size)
    {
        n = bdev->size - offset;
    }

    size_t first_block = offset / BLOCK_SIZE;
    size_t last_block = (offset + n - 1) / BLOCK_SIZE;

    size_t rel_start = offset % BLOCK_SIZE;
    size_t copied = 0;

    struct process *proc = get_current();

    for (size_t i = first_block; i <= last_block; ++i)
    {
        struct buf *bp = bio_read(bdev->dev.device_number, i);

        size_t to_copy = BLOCK_SIZE - rel_start;
        if (copied + to_copy > n)
        {
            to_copy = n - copied;
        }

        if (do_read)
        {
            if (uvm_copy_out(proc->pagetable, addr_u + rel_start,
                             (char *)(bp->data + rel_start), to_copy) == -1)
            {
                return -1;
            }
        }
        else
        {
            if (uvm_copy_in(proc->pagetable, (char *)(bp->data + rel_start),
                            addr_u + rel_start, n) == -1)
            {
                return -1;
            }
        }
        copied += to_copy;
        rel_start = 0;

        bio_write(bp);
        bio_release(bp);
    }

    return n;
}

ssize_t block_device_read(struct Block_Device *bdev, size_t addr_u,
                          size_t offset, size_t n)
{
    return block_device_rw(bdev, addr_u, offset, n, true);
}

ssize_t block_device_write(struct Block_Device *bdev, size_t addr_u,
                           size_t offset, size_t n)
{
    return block_device_rw(bdev, addr_u, offset, n, false);
}
