/* SPDX-License-Identifier: MIT */
#pragma once

#include <drivers/virtio.h>
#include <kernel/buf.h>
#include <kernel/kernel.h>
#include <kernel/spinlock.h>

struct virtio_disk
{
    /// a set (not a ring) of DMA descriptors, with which the
    /// driver tells the device where to read and write individual
    /// disk operations. there are VIRTIO_DESCRIPTORS descriptors.
    /// most commands consist of a "chain" (a linked list) of a couple of
    /// these descriptors.
    struct virtq_desc *desc;

    /// a ring in which the driver writes descriptor numbers
    /// that the driver would like the device to process.  it only
    /// includes the head descriptor of each chain. the ring has
    /// VIRTIO_DESCRIPTORS elements.
    struct virtq_avail *avail;

    /// a ring in which the device writes descriptor numbers that
    /// the device has finished processing (just the head of each chain).
    /// there are VIRTIO_DESCRIPTORS used ring entries.
    struct virtq_used *used;

    /// our own book-keeping.
    char free[VIRTIO_DESCRIPTORS];  // is a descriptor free?
    uint16_t used_idx;  // we've looked this far in used[2..VIRTIO_DESCRIPTORS].

    /// track info about in-flight operations,
    /// for use when completion interrupt arrives.
    /// indexed by first descriptor index of chain.
    struct
    {
        struct buf *b;
        char status;
    } info[VIRTIO_DESCRIPTORS];

    /// disk command headers.
    /// one-for-one with descriptors, for convenience.
    struct virtio_blk_req ops[VIRTIO_DESCRIPTORS];

    struct spinlock vdisk_lock;
};

/// @brief Inits the virtio disk driver (for qemu) and inits the hardware.
void virtio_disk_init();

void virtio_disk_rw(struct buf *b, bool write);

void virtio_block_device_interrupt();