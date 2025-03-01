/* SPDX-License-Identifier: MIT */

#include <drivers/generic_disc.h>
#include <drivers/ramdisk.h>
#include <kernel/major.h>
#include <kernel/param.h>
#include <kernel/proc.h>
#include <kernel/string.h>

struct ramdisk
{
    struct Generic_Disc disk;  ///< derived from a generic disk

    struct spinlock vdisk_lock;

    void *ram_base;
} g_ramdisk[MAX_MINOR_DEVICES];

size_t g_next_free_ramdisk = 0;

void *get_address_from_buffer(struct buf *b, struct ramdisk *disk)
{
    size_t disk_address = b->blockno * BLOCK_SIZE;
    void *addr = disk->ram_base + disk_address;

    if (disk_address > disk->disk.bdev.size)
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
    size_t minor = MINOR(b->dev);
    spin_lock(&g_ramdisk[minor].vdisk_lock);

    void *addr = get_address_from_buffer(b, &g_ramdisk[minor]);
    memmove(b->data, addr, BLOCK_SIZE);

    spin_unlock(&g_ramdisk[minor].vdisk_lock);
}

/// @brief Write function as mandated for a Block_Device
/// @param bd Pointer to the device
/// @param b The buffer to write out to disk.
void ramdisk_block_device_write(struct Block_Device *bd, struct buf *b)
{
    size_t minor = MINOR(b->dev);

    spin_lock(&g_ramdisk[minor].vdisk_lock);

    void *addr = get_address_from_buffer(b, &g_ramdisk[minor]);
    memmove(addr, b->data, BLOCK_SIZE);

    spin_unlock(&g_ramdisk[minor].vdisk_lock);
}

const char *ramdisk_names[MAX_MINOR_DEVICES] = {"ramdisk0", "ramdisk1",
                                                "ramdisk2", "ramdisk3"};

dev_t ramdisk_init(struct Device_Init_Parameters *init_parameters,
                   const char *name)
{
    if (init_parameters->mem[0].start == 0 || init_parameters->mem[0].size == 0)
    {
        panic("invalid ramdisk_init parameters");
    }
    size_t minor = g_next_free_ramdisk++;
    // printk("ramdisk_init %zd\n", minor);

    g_ramdisk[minor].ram_base = (void *)init_parameters->mem[0].start;
    g_ramdisk[minor].disk.bdev.size = init_parameters->mem[0].size;

    // init device and register it in the system
    g_ramdisk[minor].disk.bdev.dev.name = ramdisk_names[minor];
    g_ramdisk[minor].disk.bdev.dev.type = BLOCK;
    g_ramdisk[minor].disk.bdev.dev.device_number = MKDEV(RAMDISK_MAJOR, minor);
    g_ramdisk[minor].disk.bdev.ops.read_buf = ramdisk_block_device_read;
    g_ramdisk[minor].disk.bdev.ops.write_buf = ramdisk_block_device_write;
    dev_set_irq(&g_ramdisk[minor].disk.bdev.dev, INVALID_IRQ_NUMBER, NULL);
    register_device(&g_ramdisk[minor].disk.bdev.dev);

    return MKDEV(RAMDISK_MAJOR, minor);
}