/* SPDX-License-Identifier: MIT */
#pragma once

#include <kernel/kernel.h>

struct virtio_blk_config
{
    // The capacity (in 512-byte sectors)
    uint64_t capacity;
    // The maximum segment size (if VIRTIO_BLK_F_SIZE_MAX)
    uint32_t size_max;
    // The maximum number of segments (if VIRTIO_BLK_F_SEG_MAX)
    uint32_t seg_max;
    // geometry the device (if VIRTIO_BLK_F_GEOMETRY)
    struct virtio_blk_geometry
    {
        uint16_t cylinders;
        uint8_t heads;
        uint8_t sectors;
    } geometry;
    // block size of device (if VIRTIO_BLK_F_BLK_SIZE)
    uint32_t blk_size;
} __attribute__((packed));

// these are specific to virtio block devices, e.g. disks,
// described in Section 5.2 of the spec.

#define VIRTIO_BLK_T_IN 0   ///< read the disk
#define VIRTIO_BLK_T_OUT 1  ///< write the disk

/// the format of the first descriptor in a disk request.
/// to be followed by two more descriptors containing
/// the block, and a one-byte status.
struct virtio_blk_req
{
    uint32_t type;  ///< VIRTIO_BLK_T_IN or ..._OUT
    uint32_t reserved;
    uint64_t sector;
};
