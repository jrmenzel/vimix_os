/* SPDX-License-Identifier: MIT */

#include <drivers/generic_disc.h>
#include <drivers/ramdisk.h>
#include <kernel/container_of.h>
#include <kernel/kalloc.h>
#include <kernel/kernel.h>
#include <kernel/major.h>
#include <kernel/proc.h>
#include <kernel/stdatomic.h>
#include <kernel/string.h>

struct ramdisk
{
    struct Generic_Disc disk;  ///< derived from a generic disk

    struct spinlock vdisk_lock;

    void *ram_base;
};

#define ramdisk_from_generic_disk(ptr) container_of(ptr, struct ramdisk, disk)

atomic_size_t g_ramdisk_next_minor = 0;

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
    struct Generic_Disc *gdisk = generic_disk_from_block_device(bd);
    struct ramdisk *rdisk = ramdisk_from_generic_disk(gdisk);

    spin_lock(&rdisk->vdisk_lock);

    void *addr = get_address_from_buffer(b, rdisk);
    memmove(b->data, addr, BLOCK_SIZE);

    spin_unlock(&rdisk->vdisk_lock);
}

/// @brief Write function as mandated for a Block_Device
/// @param bd Pointer to the device
/// @param b The buffer to write out to disk.
void ramdisk_block_device_write(struct Block_Device *bd, struct buf *b)
{
    struct Generic_Disc *gdisk = generic_disk_from_block_device(bd);
    struct ramdisk *rdisk = ramdisk_from_generic_disk(gdisk);

    spin_lock(&rdisk->vdisk_lock);

    void *addr = get_address_from_buffer(b, rdisk);
    memmove(addr, b->data, BLOCK_SIZE);

    spin_unlock(&rdisk->vdisk_lock);
}

dev_t ramdisk_init(struct Device_Init_Parameters *init_parameters,
                   const char *name)
{
    if (init_parameters->mem[0].start == 0 || init_parameters->mem[0].size == 0)
    {
        panic("invalid ramdisk_init parameters");
    }

    struct ramdisk *rdisk = kmalloc(sizeof(struct ramdisk));
    if (rdisk == NULL)
    {
        printk("ramdisk: out of memory\n");
        return INVALID_DEVICE;
    }
    memset(rdisk, 0, sizeof(struct ramdisk));

    size_t minor = (size_t)atomic_fetch_add(&g_ramdisk_next_minor, 1);
    // printk("ramdisk_init %zd\n", minor);

    rdisk->ram_base = (void *)init_parameters->mem[0].start;
    rdisk->disk.bdev.size = init_parameters->mem[0].size;

    char *device_name = kmalloc(16);
    if (device_name == NULL)
    {
        kfree(rdisk);
        printk("ramdisk: out of memory\n");
        return INVALID_DEVICE;
    }
    snprintf(device_name, 16, "ramdisk%zd", minor);

    // init device and register it in the system
    dev_init(&rdisk->disk.bdev.dev, BLOCK, MKDEV(RAMDISK_MAJOR, minor),
             device_name, INVALID_IRQ_NUMBER, NULL);
    rdisk->disk.bdev.ops.read_buf = ramdisk_block_device_read;
    rdisk->disk.bdev.ops.write_buf = ramdisk_block_device_write;
    register_device(&rdisk->disk.bdev.dev);

    return MKDEV(RAMDISK_MAJOR, minor);
}