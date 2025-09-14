/* SPDX-License-Identifier: MIT */

#include <arch/interrupts.h>
#include <drivers/block_device.h>
#include <drivers/character_device.h>
#include <drivers/device.h>
#include <kernel/bio.h>
#include <kernel/kernel.h>
#include <kernel/proc.h>

void dev_init(struct Device *dev, device_type type, dev_t device_number,
              const char *name, int32_t irq_number,
              interrupt_handler_p interrupt_handler)
{
    dev->type = type;
    dev->irq_number = irq_number;
    dev->dev_ops.interrupt_handler = interrupt_handler;
    dev->device_number = device_number;
    dev->name = name;

    // init kobject
    kobject_init(&dev->kobj, NULL);
}

struct Device *dev_by_device_number(dev_t device_number)
{
    DEBUG_EXTRA_PANIC((MAJOR(device_number) < MAX_MAJOR_DEVICE_NUMBER),
                      "invalid device number");

    rwspin_read_lock(&g_kobjects_dev.children_lock);
    struct list_head *pos;
    list_for_each(pos, &g_kobjects_dev.children)
    {
        struct kobject *kobj = kobject_from_child_list(pos);
        struct Device *dev = device_from_kobj(kobj);
        if (dev->device_number == device_number)
        {
            rwspin_read_unlock(&g_kobjects_dev.children_lock);
            return dev;
        }
    }
    rwspin_read_unlock(&g_kobjects_dev.children_lock);

    return NULL;
}

struct Device *dev_by_irq_number(int32_t irq_number)
{
    rwspin_read_lock(&g_kobjects_dev.children_lock);
    struct list_head *pos;
    list_for_each(pos, &g_kobjects_dev.children)
    {
        struct kobject *kobj = kobject_from_child_list(pos);
        struct Device *dev = device_from_kobj(kobj);
        if (dev->irq_number == irq_number)
        {
            rwspin_read_unlock(&g_kobjects_dev.children_lock);
            return dev;
        }
    }
    rwspin_read_unlock(&g_kobjects_dev.children_lock);

    return NULL;
}

void register_device(struct Device *dev)
{
    if ((MAJOR(dev->device_number) >= MAX_MAJOR_DEVICE_NUMBER))
    {
        panic("invalid high device number");
    }
    if (dev_exists(dev->device_number))
    {
        panic("multiple drivers with same device number");
    }

    // hook up interrupts:
    if (dev->irq_number != INVALID_IRQ_NUMBER)
    {
        interrupt_controller_set_interrupt_priority(dev->irq_number, 1);
        // printk("register device %d with IRQ %d\n", dev->device_number,
        //        dev->irq_number);
    }

    kobject_add(&dev->kobj, &g_kobjects_dev, dev->name);
    // this device had a reference since kobject_init(),
    // kobject_add() added another one for the parent, which
    // from now on will be the only one
    kobject_put(&dev->kobj);
}

void dev_set_irq(struct Device *dev, int32_t irq_number,
                 interrupt_handler_p interrupt_handler)
{
    dev->irq_number = irq_number;
    dev->dev_ops.interrupt_handler = interrupt_handler;
}

bool dev_exists(dev_t device_number)
{
    return (dev_by_device_number(device_number) != NULL);
}

#ifdef CONFIG_DEBUG_EXTRA_RUNTIME_TESTS
#define DEVICE_IS_OK(major, minor, TYPE)                           \
    if ((dev_exists(MKDEV(major, minor)) == false) ||              \
        (dev_by_device_number(MKDEV(major, minor))->type != TYPE)) \
    {                                                              \
        panic("no device of that type");                           \
        return NULL;                                               \
    }
#else
#define DEVICE_IS_OK(major, minor, TYPE)
#endif

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

ssize_t character_device_read_unsupported(struct Device *dev,
                                          bool addr_is_userspace, size_t addr,
                                          size_t len)
{
    return -1;
}

ssize_t character_device_write_unsupported(struct Device *dev,
                                           bool addr_is_userspace, size_t addr,
                                           size_t len)
{
    return len;
}
