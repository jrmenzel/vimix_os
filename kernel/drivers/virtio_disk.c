/* SPDX-License-Identifier: MIT */

//
// driver for qemu's virtio disk device.
// uses qemu's mmio interface to virtio.
//
// qemu ... -drive file=fs.img,if=none,format=raw,id=x0 -device
// virtio-blk-device,drive=x0,bus=virtio-mmio-bus.0
//

#include <drivers/virtio_disk.h>
#include <kernel/buf.h>
#include <kernel/cpu.h>
#include <kernel/fs.h>
#include <kernel/kalloc.h>
#include <kernel/kernel.h>
#include <kernel/printk.h>
#include <kernel/proc.h>
#include <kernel/sleeplock.h>
#include <kernel/spinlock.h>
#include <kernel/string.h>
#include <mm/memlayout.h>

/// the address of virtio mmio register r.
#define R(r) ((volatile uint32 *)(VIRTIO0 + (r)))

struct virtio_disk g_virtio_disk;

void virtio_disk_init()
{
    uint32 status = 0;

    spin_lock_init(&g_virtio_disk.vdisk_lock, "virtio_disk");

    if (*R(VIRTIO_MMIO_MAGIC_VALUE) != 0x74726976 ||
        *R(VIRTIO_MMIO_VERSION) != 2 || *R(VIRTIO_MMIO_DEVICE_ID) != 2 ||
        *R(VIRTIO_MMIO_VENDOR_ID) != 0x554d4551)
    {
        panic("could not find virtio disk");
    }

    // reset device
    *R(VIRTIO_MMIO_STATUS) = status;

    // set ACKNOWLEDGE status bit
    status |= VIRTIO_CONFIG_S_ACKNOWLEDGE;
    *R(VIRTIO_MMIO_STATUS) = status;

    // set DRIVER status bit
    status |= VIRTIO_CONFIG_S_DRIVER;
    *R(VIRTIO_MMIO_STATUS) = status;

    // negotiate features
    uint64 features = *R(VIRTIO_MMIO_DEVICE_FEATURES);
    features &= ~(1 << VIRTIO_BLK_F_RO);
    features &= ~(1 << VIRTIO_BLK_F_SCSI);
    features &= ~(1 << VIRTIO_BLK_F_CONFIG_WCE);
    features &= ~(1 << VIRTIO_BLK_F_MQ);
    features &= ~(1 << VIRTIO_F_ANY_LAYOUT);
    features &= ~(1 << VIRTIO_RING_F_EVENT_IDX);
    features &= ~(1 << VIRTIO_RING_F_INDIRECT_DESC);
    *R(VIRTIO_MMIO_DRIVER_FEATURES) = features;

    // tell device that feature negotiation is complete.
    status |= VIRTIO_CONFIG_S_FEATURES_OK;
    *R(VIRTIO_MMIO_STATUS) = status;

    // re-read status to ensure FEATURES_OK is set.
    status = *R(VIRTIO_MMIO_STATUS);
    if (!(status & VIRTIO_CONFIG_S_FEATURES_OK))
        panic("virtio disk FEATURES_OK unset");

    // initialize queue 0.
    *R(VIRTIO_MMIO_QUEUE_SEL) = 0;

    // ensure queue 0 is not in use.
    if (*R(VIRTIO_MMIO_QUEUE_READY)) panic("virtio disk should not be ready");

    // check maximum queue size.
    uint32 max = *R(VIRTIO_MMIO_QUEUE_VIRTIO_DESCRIPTORS_MAX);
    if (max == 0) panic("virtio disk has no queue 0");
    if (max < VIRTIO_DESCRIPTORS) panic("virtio disk max queue too short");

    // allocate and zero queue memory.
    g_virtio_disk.desc = kalloc();
    g_virtio_disk.avail = kalloc();
    g_virtio_disk.used = kalloc();
    if (!g_virtio_disk.desc || !g_virtio_disk.avail || !g_virtio_disk.used)
        panic("virtio disk kalloc");
    memset(g_virtio_disk.desc, 0, PAGE_SIZE);
    memset(g_virtio_disk.avail, 0, PAGE_SIZE);
    memset(g_virtio_disk.used, 0, PAGE_SIZE);

    // set queue size.
    *R(VIRTIO_MMIO_QUEUE_VIRTIO_DESCRIPTORS) = VIRTIO_DESCRIPTORS;

    // write physical addresses.
    *R(VIRTIO_MMIO_QUEUE_DESC_LOW) = (uint64)g_virtio_disk.desc;
    *R(VIRTIO_MMIO_QUEUE_DESC_HIGH) = (uint64)g_virtio_disk.desc >> 32;
    *R(VIRTIO_MMIO_DRIVER_DESC_LOW) = (uint64)g_virtio_disk.avail;
    *R(VIRTIO_MMIO_DRIVER_DESC_HIGH) = (uint64)g_virtio_disk.avail >> 32;
    *R(VIRTIO_MMIO_DEVICE_DESC_LOW) = (uint64)g_virtio_disk.used;
    *R(VIRTIO_MMIO_DEVICE_DESC_HIGH) = (uint64)g_virtio_disk.used >> 32;

    // queue is ready.
    *R(VIRTIO_MMIO_QUEUE_READY) = 0x1;

    // all VIRTIO_DESCRIPTORS descriptors start out unused.
    for (int i = 0; i < VIRTIO_DESCRIPTORS; i++) g_virtio_disk.free[i] = 1;

    // tell device we're completely ready.
    status |= VIRTIO_CONFIG_S_DRIVER_OK;
    *R(VIRTIO_MMIO_STATUS) = status;

    // plic.c and trap.c arrange for interrupts from VIRTIO0_IRQ.
}

/// find a free descriptor, mark it non-free, return its index.
static int alloc_desc()
{
    for (int i = 0; i < VIRTIO_DESCRIPTORS; i++)
    {
        if (g_virtio_disk.free[i])
        {
            g_virtio_disk.free[i] = 0;
            return i;
        }
    }
    return -1;
}

/// mark a descriptor as free.
static void free_desc(int i)
{
    if (i >= VIRTIO_DESCRIPTORS) panic("free_desc 1");
    if (g_virtio_disk.free[i]) panic("free_desc 2");
    g_virtio_disk.desc[i].addr = 0;
    g_virtio_disk.desc[i].len = 0;
    g_virtio_disk.desc[i].flags = 0;
    g_virtio_disk.desc[i].next = 0;
    g_virtio_disk.free[i] = 1;
    wakeup(&g_virtio_disk.free[0]);
}

/// free a chain of descriptors.
static void free_chain(int i)
{
    while (1)
    {
        int flag = g_virtio_disk.desc[i].flags;
        int nxt = g_virtio_disk.desc[i].next;
        free_desc(i);
        if (flag & VRING_DESC_F_NEXT)
            i = nxt;
        else
            break;
    }
}

/// allocate three descriptors (they need not be contiguous).
/// disk transfers always use three descriptors.
static int alloc3_desc(int *idx)
{
    for (int i = 0; i < 3; i++)
    {
        idx[i] = alloc_desc();
        if (idx[i] < 0)
        {
            for (int j = 0; j < i; j++) free_desc(idx[j]);
            return -1;
        }
    }
    return 0;
}

void virtio_disk_rw(struct buf *b, int write)
{
    uint64 sector = b->blockno * (BLOCK_SIZE / 512);

    spin_lock(&g_virtio_disk.vdisk_lock);

    // the spec's Section 5.2 says that legacy block operations use
    // three descriptors: one for type/reserved/sector, one for the
    // data, one for a 1-byte status result.

    // allocate the three descriptors.
    int idx[3];
    while (1)
    {
        if (alloc3_desc(idx) == 0)
        {
            break;
        }
        sleep(&g_virtio_disk.free[0], &g_virtio_disk.vdisk_lock);
    }

    // format the three descriptors.
    // qemu's virtio-blk.c reads them.

    struct virtio_blk_req *buf0 = &g_virtio_disk.ops[idx[0]];

    if (write)
        buf0->type = VIRTIO_BLK_T_OUT;  // write the disk
    else
        buf0->type = VIRTIO_BLK_T_IN;  // read the disk
    buf0->reserved = 0;
    buf0->sector = sector;

    g_virtio_disk.desc[idx[0]].addr = (uint64)buf0;
    g_virtio_disk.desc[idx[0]].len = sizeof(struct virtio_blk_req);
    g_virtio_disk.desc[idx[0]].flags = VRING_DESC_F_NEXT;
    g_virtio_disk.desc[idx[0]].next = idx[1];

    g_virtio_disk.desc[idx[1]].addr = (uint64)b->data;
    g_virtio_disk.desc[idx[1]].len = BLOCK_SIZE;
    if (write)
        g_virtio_disk.desc[idx[1]].flags = 0;  // device reads b->data
    else
        g_virtio_disk.desc[idx[1]].flags =
            VRING_DESC_F_WRITE;  // device writes b->data
    g_virtio_disk.desc[idx[1]].flags |= VRING_DESC_F_NEXT;
    g_virtio_disk.desc[idx[1]].next = idx[2];

    g_virtio_disk.info[idx[0]].status = 0xff;  // device writes 0 on success
    g_virtio_disk.desc[idx[2]].addr =
        (uint64)&g_virtio_disk.info[idx[0]].status;
    g_virtio_disk.desc[idx[2]].len = 1;
    g_virtio_disk.desc[idx[2]].flags =
        VRING_DESC_F_WRITE;  // device writes the status
    g_virtio_disk.desc[idx[2]].next = 0;

    // record struct buf for virtio_disk_intr().
    b->disk = 1;
    g_virtio_disk.info[idx[0]].b = b;

    // tell the device the first index in our chain of descriptors.
    g_virtio_disk.avail->ring[g_virtio_disk.avail->idx % VIRTIO_DESCRIPTORS] =
        idx[0];

    __sync_synchronize();

    // tell the device another avail ring entry is available.
    g_virtio_disk.avail->idx += 1;  // not % VIRTIO_DESCRIPTORS ...

    __sync_synchronize();

    *R(VIRTIO_MMIO_QUEUE_NOTIFY) = 0;  // value is queue number

    // Wait for virtio_disk_intr() to say request has finished.
    while (b->disk == 1)
    {
        sleep(b, &g_virtio_disk.vdisk_lock);
    }

    g_virtio_disk.info[idx[0]].b = 0;
    free_chain(idx[0]);

    spin_unlock(&g_virtio_disk.vdisk_lock);
}

void virtio_disk_intr()
{
    spin_lock(&g_virtio_disk.vdisk_lock);

    // the device won't raise another interrupt until we tell it
    // we've seen this interrupt, which the following line does.
    // this may race with the device writing new entries to
    // the "used" ring, in which case we may process the new
    // completion entries in this interrupt, and have nothing to do
    // in the next interrupt, which is harmless.
    *R(VIRTIO_MMIO_INTERRUPT_ACK) = *R(VIRTIO_MMIO_INTERRUPT_STATUS) & 0x3;

    __sync_synchronize();

    // the device increments disk.used->idx when it
    // adds an entry to the used ring.

    while (g_virtio_disk.used_idx != g_virtio_disk.used->idx)
    {
        __sync_synchronize();
        int id = g_virtio_disk.used
                     ->ring[g_virtio_disk.used_idx % VIRTIO_DESCRIPTORS]
                     .id;

        if (g_virtio_disk.info[id].status != 0)
            panic("virtio_disk_intr status");

        struct buf *b = g_virtio_disk.info[id].b;
        b->disk = 0;  // disk is done with buf
        wakeup(b);

        g_virtio_disk.used_idx += 1;
    }

    spin_unlock(&g_virtio_disk.vdisk_lock);
}
