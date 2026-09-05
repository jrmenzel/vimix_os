/* SPDX-License-Identifier: MIT */
#pragma once

#include <drivers/driver_list.h>
#include <kernel/kernel.h>

#define VIRTIO_DEVICE_ID_INVALID 0
#define VIRTIO_DEVICE_ID_NET 1
#define VIRTIO_DEVICE_ID_BLOCK 2
#define VIRTIO_DEVICE_ID_CONSOLE 3
#define VIRTIO_DEVICE_ID_ENTROPY 4
#define VIRTIO_DEVICE_ID_GPU 16

#define VIRTIO_MMIO_MAGIC 0x74726976
#define VIRTIO_MMIO_MAGIC_VALUE 0x000
#define VIRTIO_MMIO_VERSION 0x004
#define VIRTIO_MMIO_DEVICE_ID 0x008
#define VIRTIO_MMIO_VENDOR_ID 0x00c
#define VIRTIO_MMIO_DEVICE_FEATURES 0x010
#define VIRTIO_MMIO_DEVICE_FEATURES_SEL 0x014
#define VIRTIO_MMIO_DRIVER_FEATURES 0x020
#define VIRTIO_MMIO_DRIVER_FEATURES_SEL 0x024
#define VIRTIO_MMIO_QUEUE_SEL 0x030
#define VIRTIO_MMIO_QUEUE_NUM_MAX 0x034
#define VIRTIO_MMIO_QUEUE_NUM 0x038
#define VIRTIO_MMIO_QUEUE_READY 0x044
#define VIRTIO_MMIO_QUEUE_NOTIFY 0x050
#define VIRTIO_MMIO_INTERRUPT_STATUS 0x060
#define VIRTIO_MMIO_INTERRUPT_ACK 0x064
#define VIRTIO_MMIO_STATUS 0x070
#define VIRTIO_MMIO_QUEUE_DESC_LOW 0x080
#define VIRTIO_MMIO_QUEUE_DESC_HIGH 0x084
#define VIRTIO_MMIO_DRIVER_DESC_LOW 0x090
#define VIRTIO_MMIO_DRIVER_DESC_HIGH 0x094
#define VIRTIO_MMIO_DEVICE_DESC_LOW 0x0a0
#define VIRTIO_MMIO_DEVICE_DESC_HIGH 0x0a4
#define VIRTIO_MMIO_CONFIG 0x100

#define VIRTIO_CONFIG_S_ACKNOWLEDGE 1
#define VIRTIO_CONFIG_S_DRIVER 2
#define VIRTIO_CONFIG_S_DRIVER_OK 4
#define VIRTIO_CONFIG_S_FEATURES_OK 8
#define VIRTIO_CONFIG_S_FAILED 128

#define VIRTIO_F_ANY_LAYOUT 27
#define VIRTIO_RING_F_INDIRECT_DESC 28
#define VIRTIO_RING_F_EVENT_IDX 29
#define VIRTIO_F_VERSION_1 32
#define VIRTIO_FEATURE(bit) ((uint64_t)1 << (bit))

/// Number of descriptors in each queue. Must be a power of two.
#define VIRTIO_DESCRIPTORS 8

struct virtq_desc
{
    uint64_t addr;
    uint32_t len;
    uint16_t flags;
    uint16_t next;
};
#define VRING_DESC_F_NEXT 1
#define VRING_DESC_F_WRITE 2

struct virtq_avail
{
    uint16_t flags;
    uint16_t idx;
    uint16_t ring[VIRTIO_DESCRIPTORS];
    uint16_t unused;
};

struct virtq_used_elem
{
    uint32_t id;
    uint32_t len;
};

struct virtq_used
{
    uint16_t flags;
    uint16_t idx;
    struct virtq_used_elem ring[VIRTIO_DESCRIPTORS];
};

struct Virtio_Device
{
    size_t mmio_base;
    uint32_t status;
    uint64_t features;
};

struct Virtio_Queue
{
    struct Virtio_Device *device;
    uint16_t number;
    struct virtq_desc *desc;
    struct virtq_avail *avail;
    struct virtq_used *used;
    char free[VIRTIO_DESCRIPTORS];
    uint16_t used_idx;
};

bool virtio_mmio_is_device(struct Device_Init_Parameters *params,
                           uint32_t device_id);
bool virtio_device_begin(struct Virtio_Device *device,
                         struct Device_Init_Parameters *params,
                         uint32_t device_id, uint64_t supported_features);
void virtio_device_finish(struct Virtio_Device *device);
void virtio_device_fail(struct Virtio_Device *device);

bool virtio_queue_init(struct Virtio_Device *device, struct Virtio_Queue *queue,
                       uint16_t number);
void virtio_queue_destroy(struct Virtio_Queue *queue);
int32_t virtio_queue_alloc_desc(struct Virtio_Queue *queue);
void virtio_queue_free_desc(struct Virtio_Queue *queue, int32_t index);
void virtio_queue_free_chain(struct Virtio_Queue *queue, int32_t index);
void virtio_queue_notify(struct Virtio_Queue *queue);
void virtio_queue_submit(struct Virtio_Queue *queue, uint16_t head);
bool virtio_queue_pop_used(struct Virtio_Queue *queue,
                           struct virtq_used_elem *element);
uint32_t virtio_device_ack_interrupt(struct Virtio_Device *device);
