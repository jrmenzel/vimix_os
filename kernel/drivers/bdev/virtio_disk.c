/* SPDX-License-Identifier: MIT */

// driver for qemu's virtio disk device.
// uses qemu's mmio interface to virtio.
//
// qemu ... -drive file=fs.img,if=none,format=raw,id=x0 -device
// virtio-blk-device,drive=x0,bus=virtio-mmio-bus.0

#include <drivers/bdev/virtio_blk.h>
#include <drivers/bdev/virtio_disk.h>
#include <drivers/bdev/virtio_disk_sysfs.h>
#include <drivers/driver.h>
#include <kernel/buf.h>
#include <kernel/fs.h>
#include <kernel/kernel.h>
#include <kernel/proc.h>
#include <kernel/sleeplock.h>
#include <kernel/spinlock.h>
#include <kernel/stdatomic.h>
#include <kernel/string.h>
#include <mm/kalloc.h>

REGISTER_DRIVER("virtio,mmio", virtio_disk_init);

atomic_size_t g_virtio_disk_next_minor = 0;

void virtio_block_device_read(struct Block_Device *bd, struct buf *b);
void virtio_block_device_write(struct Block_Device *bd, struct buf *b);
syserr_t virtio_block_device_flush(struct Block_Device *bd);
void virtio_block_device_interrupt(dev_t dev);

dev_t virtio_disk_init(struct Device_Init_Parameters *init_parameters,
                       const char *name)
{
    DRIVER_CHECK_INIT_PARAMS(init_parameters);

    if (!virtio_mmio_is_device(init_parameters, VIRTIO_DEVICE_ID_BLOCK))
    {
        // not a virtio disk (or no file attached via qemu)
        return INVALID_DEVICE;
    }

    struct Virtio_Disk *disk =
        kmalloc(sizeof(struct Virtio_Disk), ALLOC_FLAG_ZERO_MEMORY);
    if (disk == NULL)
    {
        printk("virtio disk: out of memory\n");
        return INVALID_DEVICE;
    }

    syserr_t err =
        dev_init(&disk->disk.bdev.dev, BLOCK, QEMU_VIRT_IO_DISK_MAJOR,
                 &g_virtio_disk_next_minor, "virtio_disk",
                 init_parameters->interrupts, init_parameters->interrupt_count,
                 virtio_block_device_interrupt, &virtio_disk_kobj_ktype);
    if (err != 0)
    {
        virtio_device_fail(&disk->virtio);
        kfree(disk);
        return INVALID_DEVICE;
    }

    spin_lock_init(&disk->vdisk_lock, "virtio_disk");
    uint64_t supported_features = VIRTIO_FEATURE(VIRTIO_BLK_F_FLUSH);
    if (!virtio_device_begin(&disk->virtio, init_parameters,
                             VIRTIO_DEVICE_ID_BLOCK, supported_features) ||
        !virtio_queue_init(&disk->virtio, &disk->requestq, 0))
    {
        virtio_device_fail(&disk->virtio);
        virtio_queue_destroy(&disk->requestq);
        kfree((void *)disk->disk.bdev.dev.name);
        kfree(disk);
        return INVALID_DEVICE;
    }
    virtio_device_finish(&disk->virtio);

    struct virtio_blk_config *config =
        (struct virtio_blk_config *)(disk->virtio.mmio_base +
                                     VIRTIO_MMIO_CONFIG);

    disk->disk.bdev.size = config->capacity * 512;
    disk->disk.bdev.ops.read_buf = virtio_block_device_read;
    disk->disk.bdev.ops.write_buf = virtio_block_device_write;
    disk->disk.bdev.ops.flush = virtio_block_device_flush;
    disk->disk.bdev.dev.mode = 0600;

    register_device(&disk->disk.bdev.dev);

    return disk->disk.bdev.dev.device_number;
}

/// allocate three descriptors (they need not be contiguous).
/// disk transfers always use three descriptors.
static int32_t alloc3_desc(struct Virtio_Disk *disk, int32_t *idx)
{
    for (size_t i = 0; i < 3; i++)
    {
        // find a free descriptor, mark it non-free, return its index.
        idx[i] = virtio_queue_alloc_desc(&disk->requestq);
        if (idx[i] < 0)
        {
            for (size_t j = 0; j < i; j++)
            {
                virtio_queue_free_desc(&disk->requestq, idx[j]);
            }
            return -1;
        }
    }
    return 0;
}

void virtio_disk_rw(struct Virtio_Disk *disk, struct buf *b, bool write)
{
    uint64_t sector = b->blockno * (BLOCK_SIZE / 512);
    uint64_t sector_count = disk->disk.bdev.size / 512;
    if (sector >= sector_count)
    {
        panic("virtio_disk_rw: invalid sector");
    }
    size_t read_amount = BLOCK_SIZE;
    if (sector == sector_count - 1)
    {
        // A disk with an uneven number of sectors can't read two
        // sectors ( == 1 block )
        read_amount = 512;
    }

    spin_lock(&disk->vdisk_lock);

    // the spec's Section 5.2 says that legacy block operations use
    // three descriptors: one for type/reserved/sector, one for the
    // data, one for a 1-byte status result.

    // allocate the three descriptors.
    int32_t idx[3];
    while (true)
    {
        if (alloc3_desc(disk, idx) == 0)
        {
            break;
        }
        sleep(&disk->requestq.free[0], &disk->vdisk_lock);
    }

    // format the three descriptors.
    // qemu's virtio-blk.c reads them.

    struct virtio_blk_req *buf0 = &disk->ops[idx[0]];

    if (write)
    {
        buf0->type = VIRTIO_BLK_T_OUT;  // write the disk
    }
    else
    {
        buf0->type = VIRTIO_BLK_T_IN;  // read the disk
    }
    buf0->reserved = 0;
    buf0->sector = sector;

    struct virtq_desc *desc = disk->requestq.desc;
    desc[idx[0]].addr = virt_to_phys((size_t)buf0);
    desc[idx[0]].len = sizeof(struct virtio_blk_req);
    desc[idx[0]].flags = VRING_DESC_F_NEXT;
    desc[idx[0]].next = idx[1];

    desc[idx[1]].addr = virt_to_phys((size_t)b->data);
    desc[idx[1]].len = read_amount;
    if (write)
    {
        desc[idx[1]].flags = 0;  // device reads b->data
    }
    else
    {
        desc[idx[1]].flags = VRING_DESC_F_WRITE;  // device writes b->data
    }
    desc[idx[1]].flags |= VRING_DESC_F_NEXT;
    desc[idx[1]].next = idx[2];

    disk->info[idx[0]].status = 0xff;  // device writes 0 on success
    desc[idx[2]].addr = virt_to_phys((size_t)&disk->info[idx[0]].status);
    desc[idx[2]].len = 1;
    // device writes the status:
    desc[idx[2]].flags = VRING_DESC_F_WRITE;
    desc[idx[2]].next = 0;

    // record struct buf for virtio_block_device_interrupt().
    b->owned_by_driver = true;
    disk->info[idx[0]].b = b;
    disk->info[idx[0]].completed = false;

    virtio_queue_submit(&disk->requestq, idx[0]);

    // Wait for virtio_block_device_interrupt() to say request has finished.
    while (b->owned_by_driver == true)
    {
        sleep(b, &disk->vdisk_lock);
    }

    disk->info[idx[0]].b = 0;
    virtio_queue_free_chain(&disk->requestq, idx[0]);

    spin_unlock(&disk->vdisk_lock);
}

syserr_t virtio_block_device_flush(struct Block_Device *bd)
{
    struct Generic_Disc *gdisk = generic_disk_from_block_device(bd);
    struct Virtio_Disk *disk = virtio_disk_from_generic_disk(gdisk);

    if (!(disk->virtio.features & VIRTIO_FEATURE(VIRTIO_BLK_F_FLUSH)))
    {
        return -EOTHER;
    }

    spin_lock(&disk->vdisk_lock);

    int32_t request = virtio_queue_alloc_desc(&disk->requestq);
    int32_t status = virtio_queue_alloc_desc(&disk->requestq);
    if (request < 0 || status < 0)
    {
        if (request >= 0) virtio_queue_free_desc(&disk->requestq, request);
        if (status >= 0) virtio_queue_free_desc(&disk->requestq, status);
        spin_unlock(&disk->vdisk_lock);
        return -EOTHER;
    }

    struct virtio_blk_req *header = &disk->ops[request];
    header->type = VIRTIO_BLK_T_FLUSH;
    header->reserved = 0;
    header->sector = 0;

    struct virtq_desc *desc = disk->requestq.desc;
    desc[request].addr = virt_to_phys((size_t)header);
    desc[request].len = sizeof(*header);
    desc[request].flags = VRING_DESC_F_NEXT;
    desc[request].next = status;

    disk->info[request].b = NULL;
    disk->info[request].status = 0xff;
    disk->info[request].completed = false;
    desc[status].addr = virt_to_phys((size_t)&disk->info[request].status);
    desc[status].len = 1;
    desc[status].flags = VRING_DESC_F_WRITE;
    desc[status].next = 0;

    virtio_queue_submit(&disk->requestq, request);
    while (!disk->info[request].completed)
    {
        sleep(&disk->info[request].completed, &disk->vdisk_lock);
    }

    syserr_t result = disk->info[request].status == 0 ? 0 : -EOTHER;
    virtio_queue_free_chain(&disk->requestq, request);
    spin_unlock(&disk->vdisk_lock);
    return result;
}

/// @brief Read function as mandated for a Block_Device
/// @param bd Pointer to the device
/// @param b The buffer to fill.
void virtio_block_device_read(struct Block_Device *bd, struct buf *b)
{
    struct Generic_Disc *gdisk = generic_disk_from_block_device(bd);
    struct Virtio_Disk *vdisk = virtio_disk_from_generic_disk(gdisk);

    virtio_disk_rw(vdisk, b, false);
}

/// @brief Write function as mandated for a Block_Device
/// @param bd Pointer to the device
/// @param b The buffer to write out to disk.
void virtio_block_device_write(struct Block_Device *bd, struct buf *b)
{
    struct Generic_Disc *gdisk = generic_disk_from_block_device(bd);
    struct Virtio_Disk *vdisk = virtio_disk_from_generic_disk(gdisk);

    virtio_disk_rw(vdisk, b, true);
}

/// @brief The interrupt handler for the Block_Device
void virtio_block_device_interrupt(dev_t dev)
{
    struct Block_Device *bd = get_block_device(dev);
    struct Generic_Disc *gdisk = generic_disk_from_block_device(bd);
    struct Virtio_Disk *disk = virtio_disk_from_generic_disk(gdisk);

    spin_lock(&disk->vdisk_lock);

    // the device won't raise another interrupt until we tell it
    // we've seen this interrupt, which the following line does.
    // this may race with the device writing new entries to
    // the "used" ring, in which case we may process the new
    // completion entries in this interrupt, and have nothing to do
    // in the next interrupt, which is harmless.
    virtio_device_ack_interrupt(&disk->virtio);

    struct virtq_used_elem used;
    while (virtio_queue_pop_used(&disk->requestq, &used))
    {
        int id = used.id;

        if (disk->info[id].status != 0)
        {
            panic("virtio_block_device_interrupt status");
        }

        struct buf *b = disk->info[id].b;
        if (b != NULL)
        {
            b->owned_by_driver = false;  // disk is done with buf
            wakeup(b);
        }
        disk->info[id].completed = true;
        wakeup(&disk->info[id].completed);
    }

    spin_unlock(&disk->vdisk_lock);
}
