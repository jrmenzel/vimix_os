/* SPDX-License-Identifier: MIT */
#pragma once

#include <drivers/bdev/generic_disc.h>
#include <drivers/bdev/virtio_blk.h>
#include <drivers/device.h>
#include <drivers/devices_list.h>
#include <drivers/misc/virtio.h>
#include <kernel/buf.h>
#include <kernel/container_of.h>
#include <kernel/kernel.h>
#include <kernel/spinlock.h>

struct Virtio_Disk
{
    struct Generic_Disc disk;  ///< derived from a generic disk
    struct Virtio_Device virtio;
    struct Virtio_Queue requestq;

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

#define virtio_from_generic_disk(ptr) \
    container_of(ptr, struct Virtio_Disk, disk)

/// @brief Inits the virtio disk driver (for qemu) and inits the hardware.
/// Creates a Virtio_Disk object and adds it to the devices list.
/// @return device number of the created device
dev_t virtio_disk_init(struct Device_Init_Parameters *init_parameters,
                       const char *name);
