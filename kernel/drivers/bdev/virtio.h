/* SPDX-License-Identifier: MIT */
#pragma once

#include <kernel/kernel.h>

//
// virtio device definitions.
// for both the mmio interface, and virtio descriptors.
// only tested with qemu.
//
// the virtio spec:
// https://docs.oasis-open.org/virtio/virtio/v1.2/cs01/virtio-v1.2-cs01.html
//

// virtio mmio control registers, mapping start is read from the device tree
// from qemu virtio_mmio.h / spec 1.2 (4.2.2)
#define VIRTIO_DISK_MAGIC 0x74726976
#define VIRTIO_MMIO_MAGIC_VALUE 0x000  // VIRTIO_DISK_MAGIC
#define VIRTIO_MMIO_VERSION 0x004      // version; should be 2
#define VIRTIO_MMIO_DEVICE_ID 0x008    // device type; 1 is net, 2 is disk
#define VIRTIO_MMIO_VENDOR_ID 0x00c    // 0x554d4551
#define VIRTIO_MMIO_DEVICE_FEATURES \
    0x010  // Flags representing features the device supports
#define VIRTIO_MMIO_DEVICE_FEATURES_SEL 0x14
#define VIRTIO_MMIO_DRIVER_FEATURES \
    0x020  // features understood and activated by the driver
#define VIRTIO_MMIO_QUEUE_SEL 0x030      // select queue, write-only
#define VIRTIO_MMIO_QUEUE_NUM_MAX 0x034  // max size of current queue, read-only
#define VIRTIO_MMIO_QUEUE_NUM 0x038      // size of current queue, write-only
#define VIRTIO_MMIO_QUEUE_READY 0x044    // ready bit
#define VIRTIO_MMIO_QUEUE_NOTIFY 0x050   // write-only
#define VIRTIO_MMIO_INTERRUPT_STATUS 0x060  // read-only
#define VIRTIO_MMIO_INTERRUPT_ACK 0x064     // write-only
#define VIRTIO_MMIO_STATUS 0x070            // read/write
#define VIRTIO_MMIO_QUEUE_DESC_LOW \
    0x080  // physical address for descriptor table, write-only
#define VIRTIO_MMIO_QUEUE_DESC_HIGH 0x084
#define VIRTIO_MMIO_DRIVER_DESC_LOW \
    0x090  // physical address for available ring, write-only
#define VIRTIO_MMIO_DRIVER_DESC_HIGH 0x094
#define VIRTIO_MMIO_DEVICE_DESC_LOW \
    0x0a0  // physical address for used ring, write-only
#define VIRTIO_MMIO_DEVICE_DESC_HIGH 0x0a4

#define VIRTIO_MMIO_CONFIG 0x100  // beginning of struct virtio_blk_config

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

// status register bits, from qemu virtio_config.h
#define VIRTIO_CONFIG_S_ACKNOWLEDGE 1
#define VIRTIO_CONFIG_S_DRIVER 2
#define VIRTIO_CONFIG_S_DRIVER_OK 4
#define VIRTIO_CONFIG_S_FEATURES_OK 8

// device feature bits
#define VIRTIO_BLK_F_RO 5          /* Disk is read-only */
#define VIRTIO_BLK_F_SCSI 7        /* Supports scsi command passthru */
#define VIRTIO_BLK_F_CONFIG_WCE 11 /* Writeback mode available in config */
#define VIRTIO_BLK_F_MQ 12         /* support more than one vq */
#define VIRTIO_F_ANY_LAYOUT 27
#define VIRTIO_RING_F_INDIRECT_DESC 28
#define VIRTIO_RING_F_EVENT_IDX 29

/// this many virtio descriptors.
/// must be a power of two.
#define VIRTIO_DESCRIPTORS 8

/// a single descriptor, from the spec.
struct virtq_desc
{
    uint64_t addr;
    uint32_t len;
    uint16_t flags;
    uint16_t next;
};
#define VRING_DESC_F_NEXT 1   // chained with another descriptor
#define VRING_DESC_F_WRITE 2  // device writes (vs read)

/// the (entire) avail ring, from the spec.
struct virtq_avail
{
    uint16_t flags;                     ///< always zero
    uint16_t idx;                       ///< driver will write ring[idx] next
    uint16_t ring[VIRTIO_DESCRIPTORS];  ///< descriptor numbers of chain heads
    uint16_t unused;
};

/// one entry in the "used" ring, with which the
/// device tells the driver about completed requests.
struct virtq_used_elem
{
    uint32_t id;  ///< index of start of completed descriptor chain
    uint32_t len;
};

struct virtq_used
{
    uint16_t flags;  ///< always zero
    uint16_t idx;    ///< device increments when it adds a ring[] entry
    struct virtq_used_elem ring[VIRTIO_DESCRIPTORS];
};

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
