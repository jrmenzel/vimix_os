/* SPDX-License-Identifier: MIT */

// driver for qemu's virtio disk device.
// uses qemu's mmio interface to virtio.
//
// qemu ... -drive file=fs.img,if=none,format=raw,id=x0 -device
// virtio-blk-device,drive=x0,bus=virtio-mmio-bus.0

#include <drivers/mmio_access.h>
#include <drivers/virtio.h>
#include <drivers/virtio_disk.h>
#include <kernel/buf.h>
#include <kernel/fs.h>
#include <kernel/kalloc.h>
#include <kernel/kernel.h>
#include <kernel/proc.h>
#include <kernel/sleeplock.h>
#include <kernel/spinlock.h>
#include <kernel/string.h>

size_t g_virtio_disks_used = 0;
struct virtio_disk g_virtio_disks[MAX_MINOR_DEVICES];

void virtio_block_device_read(struct Block_Device *bd, struct buf *b);
void virtio_block_device_write(struct Block_Device *bd, struct buf *b);
void virtio_block_device_interrupt(dev_t dev);

const char *virtio_names[MAX_MINOR_DEVICES] = {"virtio0", "virtio1", "virtio2",
                                               "virtio3"};

dev_t virtio_disk_init_internal(size_t disk_index,
                                struct Device_Init_Parameters *mapping)
{
    if (disk_index > MAX_MINOR_DEVICES) return INVALID_DEVICE;
    struct virtio_disk *disk = &g_virtio_disks[disk_index];

    spin_lock_init(&disk->vdisk_lock, "virtio_disk");
    disk->mmio_base = mapping->mem[0].start;
    size_t b = disk->mmio_base;

    uint32_t status = 0;
    // reset device
    MMIO_WRITE_UINT_32(b, VIRTIO_MMIO_STATUS, status);

    // set ACKNOWLEDGE status bit
    status |= VIRTIO_CONFIG_S_ACKNOWLEDGE;
    MMIO_WRITE_UINT_32(b, VIRTIO_MMIO_STATUS, status);

    // set DRIVER status bit
    status |= VIRTIO_CONFIG_S_DRIVER;
    MMIO_WRITE_UINT_32(b, VIRTIO_MMIO_STATUS, status);

    // negotiate features
    uint32_t features = MMIO_READ_UINT_32(b, VIRTIO_MMIO_DEVICE_FEATURES);
    features &= ~(1 << VIRTIO_BLK_F_RO);
    features &= ~(1 << VIRTIO_BLK_F_SCSI);
    features &= ~(1 << VIRTIO_BLK_F_CONFIG_WCE);
    features &= ~(1 << VIRTIO_BLK_F_MQ);
    features &= ~(1 << VIRTIO_F_ANY_LAYOUT);
    features &= ~(1 << VIRTIO_RING_F_EVENT_IDX);
    features &= ~(1 << VIRTIO_RING_F_INDIRECT_DESC);
    MMIO_WRITE_UINT_32(b, VIRTIO_MMIO_DRIVER_FEATURES, features);

    // tell device that feature negotiation is complete.
    status |= VIRTIO_CONFIG_S_FEATURES_OK;
    MMIO_WRITE_UINT_32(b, VIRTIO_MMIO_STATUS, status);

    // re-read status to ensure FEATURES_OK is set.
    status = MMIO_READ_UINT_32(b, VIRTIO_MMIO_STATUS);
    if (!(status & VIRTIO_CONFIG_S_FEATURES_OK))
    {
        printk("ERROR: virtio disk FEATURES_OK unset\n");
        return INVALID_DEVICE;
    }

    // initialize queue 0.
    MMIO_WRITE_UINT_32(b, VIRTIO_MMIO_QUEUE_SEL, 0);

    // ensure queue 0 is not in use.
    if (MMIO_READ_UINT_32(b, VIRTIO_MMIO_QUEUE_READY))
    {
        printk("ERROR: virtio disk should not be ready\n");
        return INVALID_DEVICE;
    }

    // check maximum queue size.
    uint32_t max = MMIO_READ_UINT_32(b, VIRTIO_MMIO_QUEUE_NUM_MAX);
    if (max == 0)
    {
        printk("ERROR: virtio disk has no queue 0\n");
        return INVALID_DEVICE;
    }
    if (max < VIRTIO_DESCRIPTORS)
    {
        printk("ERROR: virtio disk max queue too short\n");
        return INVALID_DEVICE;
    }

    // allocate and zero queue memory.
    disk->desc = alloc_page(ALLOC_FLAG_ZERO_MEMORY);
    disk->avail = alloc_page(ALLOC_FLAG_ZERO_MEMORY);
    disk->used = alloc_page(ALLOC_FLAG_ZERO_MEMORY);
    if (!disk->desc || !disk->avail || !disk->used)
    {
        if (disk->desc) free_page(disk->desc);
        if (disk->avail) free_page(disk->avail);
        if (disk->used) free_page(disk->used);
        printk("ERROR: virtio disk kalloc failed\n");
        return INVALID_DEVICE;
    }
    memset(disk->desc, 0, PAGE_SIZE);
    memset(disk->avail, 0, PAGE_SIZE);
    memset(disk->used, 0, PAGE_SIZE);

    // set queue size.
    MMIO_WRITE_UINT_32(b, VIRTIO_MMIO_QUEUE_NUM, VIRTIO_DESCRIPTORS);

    // write physical addresses.
#if defined(__ARCH_32BIT)
    MMIO_WRITE_UINT_32(b, VIRTIO_MMIO_QUEUE_DESC_LOW, (size_t)disk->desc);
    MMIO_WRITE_UINT_32(b, VIRTIO_MMIO_DRIVER_DESC_LOW, (size_t)disk->avail);
    MMIO_WRITE_UINT_32(b, VIRTIO_MMIO_DEVICE_DESC_LOW, (size_t)disk->used);
#else
    MMIO_WRITE_UINT_32(b, VIRTIO_MMIO_QUEUE_DESC_LOW, (size_t)disk->desc);
    MMIO_WRITE_UINT_32(b, VIRTIO_MMIO_QUEUE_DESC_HIGH,
                       (size_t)disk->desc >> 32);
    MMIO_WRITE_UINT_32(b, VIRTIO_MMIO_DRIVER_DESC_LOW, (size_t)disk->avail);
    MMIO_WRITE_UINT_32(b, VIRTIO_MMIO_DRIVER_DESC_HIGH,
                       (size_t)disk->avail >> 32);
    MMIO_WRITE_UINT_32(b, VIRTIO_MMIO_DEVICE_DESC_LOW, (size_t)disk->used);
    MMIO_WRITE_UINT_32(b, VIRTIO_MMIO_DEVICE_DESC_HIGH,
                       (size_t)disk->used >> 32);
#endif

    // queue is ready.
    MMIO_WRITE_UINT_32(b, VIRTIO_MMIO_QUEUE_READY, 0x1);

    // all VIRTIO_DESCRIPTORS descriptors start out unused.
    for (size_t i = 0; i < VIRTIO_DESCRIPTORS; i++)
    {
        disk->free[i] = 1;
    }

    // tell device we're completely ready.
    status |= VIRTIO_CONFIG_S_DRIVER_OK;
    MMIO_WRITE_UINT_32(b, VIRTIO_MMIO_STATUS, status);

    struct virtio_blk_config *config =
        (struct virtio_blk_config *)(b + VIRTIO_MMIO_CONFIG);

    // init device and register it in the system
    disk->disk.bdev.size = config->capacity * 512;
    disk->disk.bdev.dev.name = virtio_names[disk_index];
    disk->disk.bdev.dev.type = BLOCK;
    disk->disk.bdev.dev.device_number =
        MKDEV(QEMU_VIRT_IO_DISK_MAJOR, disk_index);
    disk->disk.bdev.ops.read_buf = virtio_block_device_read;
    disk->disk.bdev.ops.write_buf = virtio_block_device_write;

    // plic.c and trap.c arrange for interrupts
    dev_set_irq(&disk->disk.bdev.dev, mapping->interrupt,
                virtio_block_device_interrupt);
    register_device(&disk->disk.bdev.dev);

    return disk->disk.bdev.dev.device_number;
}

dev_t virtio_disk_init(struct Device_Init_Parameters *init_param,
                       const char *name)
{
    size_t b = init_param->mem[0].start;

    if (MMIO_READ_UINT_32(b, VIRTIO_MMIO_MAGIC_VALUE) != VIRTIO_DISK_MAGIC ||
        MMIO_READ_UINT_32(b, VIRTIO_MMIO_VERSION) != 2 ||
        MMIO_READ_UINT_32(b, VIRTIO_MMIO_DEVICE_ID) != 2)
    {
        // no disk attached, e.g. no file specified in qemu
        return INVALID_DEVICE;
    }

    dev_t dev = virtio_disk_init_internal(g_virtio_disks_used, init_param);
    if (dev != INVALID_DEVICE)
    {
        g_virtio_disks_used++;
    }
    return dev;
}

/// find a free descriptor, mark it non-free, return its index.
static int32_t alloc_desc(struct virtio_disk *disk)
{
    for (size_t i = 0; i < VIRTIO_DESCRIPTORS; i++)
    {
        if (disk->free[i])
        {
            disk->free[i] = 0;
            return i;
        }
    }
    return -1;
}

/// mark a descriptor as free.
static void free_desc(struct virtio_disk *disk, int32_t i)
{
    if (i >= VIRTIO_DESCRIPTORS)
    {
        panic("free_desc: i too large");
    }
    if (disk->free[i])
    {
        panic("free_desc: double free");
    }
    disk->desc[i].addr = 0;
    disk->desc[i].len = 0;
    disk->desc[i].flags = 0;
    disk->desc[i].next = 0;
    disk->free[i] = 1;
    wakeup(&disk->free[0]);
}

/// free a chain of descriptors.
static void free_chain(struct virtio_disk *disk, int32_t i)
{
    while (true)
    {
        int32_t flag = disk->desc[i].flags;
        int32_t nxt = disk->desc[i].next;
        free_desc(disk, i);
        if (flag & VRING_DESC_F_NEXT)
        {
            i = nxt;
        }
        else
        {
            break;
        }
    }
}

/// allocate three descriptors (they need not be contiguous).
/// disk transfers always use three descriptors.
static int32_t alloc3_desc(struct virtio_disk *disk, int32_t *idx)
{
    for (size_t i = 0; i < 3; i++)
    {
        idx[i] = alloc_desc(disk);
        if (idx[i] < 0)
        {
            for (size_t j = 0; j < i; j++)
            {
                free_desc(disk, idx[j]);
            }
            return -1;
        }
    }
    return 0;
}

void virtio_disk_rw(struct virtio_disk *disk, struct buf *b, bool write)
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
        sleep(&disk->free[0], &disk->vdisk_lock);
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

    disk->desc[idx[0]].addr = (size_t)buf0;
    disk->desc[idx[0]].len = sizeof(struct virtio_blk_req);
    disk->desc[idx[0]].flags = VRING_DESC_F_NEXT;
    disk->desc[idx[0]].next = idx[1];

    disk->desc[idx[1]].addr = (size_t)b->data;
    disk->desc[idx[1]].len = read_amount;
    if (write)
    {
        disk->desc[idx[1]].flags = 0;  // device reads b->data
    }
    else
    {
        disk->desc[idx[1]].flags = VRING_DESC_F_WRITE;  // device writes b->data
    }
    disk->desc[idx[1]].flags |= VRING_DESC_F_NEXT;
    disk->desc[idx[1]].next = idx[2];

    disk->info[idx[0]].status = 0xff;  // device writes 0 on success
    disk->desc[idx[2]].addr = (size_t)&disk->info[idx[0]].status;
    disk->desc[idx[2]].len = 1;
    // device writes the status:
    disk->desc[idx[2]].flags = VRING_DESC_F_WRITE;
    disk->desc[idx[2]].next = 0;

    // record struct buf for virtio_block_device_interrupt().
    b->disk = 1;
    disk->info[idx[0]].b = b;

    // tell the device the first index in our chain of descriptors.
    disk->avail->ring[disk->avail->idx % VIRTIO_DESCRIPTORS] = idx[0];

    atomic_thread_fence(memory_order_seq_cst);

    // tell the device another avail ring entry is available.
    disk->avail->idx += 1;  // not % VIRTIO_DESCRIPTORS ...

    atomic_thread_fence(memory_order_seq_cst);

    uint32_t queue_number = 0;
    MMIO_WRITE_UINT_32(disk->mmio_base, VIRTIO_MMIO_QUEUE_NOTIFY, queue_number);

    // Wait for virtio_block_device_interrupt() to say request has finished.
    while (b->disk == 1)
    {
        sleep(b, &disk->vdisk_lock);
    }

    disk->info[idx[0]].b = 0;
    free_chain(disk, idx[0]);

    spin_unlock(&disk->vdisk_lock);
}

/// @brief Read function as mandated for a Block_Device
/// @param bd Pointer to the device
/// @param b The buffer to fill.
void virtio_block_device_read(struct Block_Device *bd, struct buf *b)
{
    size_t minor = MINOR(bd->dev.device_number);
    virtio_disk_rw(&g_virtio_disks[minor], b, false);
}

/// @brief Write function as mandated for a Block_Device
/// @param bd Pointer to the device
/// @param b The buffer to write out to disk.
void virtio_block_device_write(struct Block_Device *bd, struct buf *b)
{
    size_t minor = MINOR(bd->dev.device_number);
    virtio_disk_rw(&g_virtio_disks[minor], b, true);
}

/// @brief The interrupt handler for the Block_Device
void virtio_block_device_interrupt(dev_t dev)
{
    size_t minor = MINOR(dev);
    struct virtio_disk *disk = &g_virtio_disks[minor];

    spin_lock(&disk->vdisk_lock);

    // the device won't raise another interrupt until we tell it
    // we've seen this interrupt, which the following line does.
    // this may race with the device writing new entries to
    // the "used" ring, in which case we may process the new
    // completion entries in this interrupt, and have nothing to do
    // in the next interrupt, which is harmless.
    size_t b = disk->mmio_base;
    uint32_t int_status = MMIO_READ_UINT_32(b, VIRTIO_MMIO_INTERRUPT_STATUS);
    int_status &= 0x3;
    MMIO_WRITE_UINT_32(b, VIRTIO_MMIO_INTERRUPT_ACK, int_status);

    atomic_thread_fence(memory_order_seq_cst);

    // the device increments disk->used->idx when it
    // adds an entry to the used ring.

    while (disk->used_idx != disk->used->idx)
    {
        atomic_thread_fence(memory_order_seq_cst);
        int id = disk->used->ring[disk->used_idx % VIRTIO_DESCRIPTORS].id;

        if (disk->info[id].status != 0)
        {
            panic("virtio_block_device_interrupt status");
        }

        struct buf *b = disk->info[id].b;
        b->disk = 0;  // disk is done with buf
        wakeup(b);

        disk->used_idx += 1;
    }

    spin_unlock(&disk->vdisk_lock);
}
