/* SPDX-License-Identifier: MIT */

#include <drivers/generic_disc.h>
#include <drivers/ramdisk.h>
#include <kernel/major.h>
#include <kernel/param.h>
#include <kernel/string.h>

struct ramdisk
{
    struct Generic_Disc disk;  ///< derived from a generic disk

    struct ramdisk_minor
    {
        struct spinlock vdisk_lock;

        void *start;
        size_t size;
    } m[MAX_RAMDISKS];
} g_ramdisk;

size_t g_next_free_ramdisk = 0;

void *get_address_from_buffer(struct buf *b, struct ramdisk_minor *minor)
{
    size_t disk_address = b->blockno * BLOCK_SIZE;
    void *addr = minor->start + disk_address;

    if (disk_address > minor->size)
    {
        panic("ramdisk: read out of bounds");
    }

    return addr;
}

/// @brief Read function as mandated for a Block_Device
/// @param bd Pointer to the device
/// @param b The buffer to fill.
void ramdisk_block_device_read(struct Block_Device *bd, struct buf *b)
{
    size_t minor = MINOR(bd->dev.device_number);
    spin_lock(&g_ramdisk.m[minor].vdisk_lock);

    void *addr = get_address_from_buffer(b, &g_ramdisk.m[minor]);
    memmove(b->data, addr, BLOCK_SIZE);

    spin_unlock(&g_ramdisk.m[minor].vdisk_lock);
}

/// @brief Write function as mandated for a Block_Device
/// @param bd Pointer to the device
/// @param b The buffer to write out to disk.
void ramdisk_block_device_write(struct Block_Device *bd, struct buf *b)
{
    size_t minor = MINOR(bd->dev.device_number);

    spin_lock(&g_ramdisk.m[minor].vdisk_lock);

    void *addr = get_address_from_buffer(b, &g_ramdisk.m[minor]);
    memmove(addr, b->data, BLOCK_SIZE);

    spin_unlock(&g_ramdisk.m[minor].vdisk_lock);
}

dev_t ramdisk_init(struct Device_Memory_Map *mapping)
{
    if (mapping->mem_start == 0 || mapping->mem_size == 0)
    {
        panic("invalid ramdisk_init parameters");
    }
    size_t minor = g_next_free_ramdisk++;
    // printk("create ram disk %d\n", minor);

    g_ramdisk.m[minor].start = (void *)mapping->mem_start;
    g_ramdisk.m[minor].size = mapping->mem_size;

    // init device and register it in the system, but only once
    if (minor == 0)
    {
        g_ramdisk.disk.bdev.dev.type = BLOCK;
        g_ramdisk.disk.bdev.dev.device_number = MKDEV(RAMDISK_MAJOR, 0);
        g_ramdisk.disk.bdev.ops.read = ramdisk_block_device_read;
        g_ramdisk.disk.bdev.ops.write = ramdisk_block_device_write;
        dev_set_irq(&g_ramdisk.disk.bdev.dev, INVALID_IRQ_NUMBER, NULL);
        register_device(&g_ramdisk.disk.bdev.dev);
    }

    return MKDEV(RAMDISK_MAJOR, minor);
}