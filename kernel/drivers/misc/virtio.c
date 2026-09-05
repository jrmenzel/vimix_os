/* SPDX-License-Identifier: MIT */

#include <drivers/misc/virtio.h>
#include <drivers/mmio_access.h>
#include <kernel/pgtable.h>
#include <kernel/proc.h>
#include <kernel/stdatomic.h>
#include <kernel/string.h>
#include <mm/kalloc.h>

bool virtio_mmio_is_device(struct Device_Init_Parameters *params,
                           uint32_t device_id)
{
    size_t base = params->mem[0].start_va;
    return ((MMIO_READ_UINT_32(base, VIRTIO_MMIO_MAGIC_VALUE) ==
             VIRTIO_MMIO_MAGIC) &&
            (MMIO_READ_UINT_32(base, VIRTIO_MMIO_VERSION) == 2) &&
            (MMIO_READ_UINT_32(base, VIRTIO_MMIO_DEVICE_ID) == device_id));
}

static uint64_t virtio_read_features(size_t base)
{
    MMIO_WRITE_UINT_32(base, VIRTIO_MMIO_DEVICE_FEATURES_SEL, 0);
    uint32_t low = MMIO_READ_UINT_32(base, VIRTIO_MMIO_DEVICE_FEATURES);

    MMIO_WRITE_UINT_32(base, VIRTIO_MMIO_DEVICE_FEATURES_SEL, 1);
    uint32_t high = MMIO_READ_UINT_32(base, VIRTIO_MMIO_DEVICE_FEATURES);

    return ((uint64_t)high << 32) | (uint64_t)low;
}

static void virtio_write_features(size_t base, uint64_t features)
{
    uint32_t low = (uint32_t)features;
    uint32_t high = (uint32_t)(features >> 32);

    MMIO_WRITE_UINT_32(base, VIRTIO_MMIO_DRIVER_FEATURES_SEL, 0);
    MMIO_WRITE_UINT_32(base, VIRTIO_MMIO_DRIVER_FEATURES, low);

    MMIO_WRITE_UINT_32(base, VIRTIO_MMIO_DRIVER_FEATURES_SEL, 1);
    MMIO_WRITE_UINT_32(base, VIRTIO_MMIO_DRIVER_FEATURES, high);
}

bool virtio_device_begin(struct Virtio_Device *device,
                         struct Device_Init_Parameters *params,
                         uint32_t device_id, uint64_t supported_features)
{
    if (!virtio_mmio_is_device(params, device_id)) return false;

    device->mmio_base = params->mem[0].start_va;

    MMIO_WRITE_UINT_32(device->mmio_base, VIRTIO_MMIO_STATUS, 0);
    device->status = VIRTIO_CONFIG_S_ACKNOWLEDGE;
    MMIO_WRITE_UINT_32(device->mmio_base, VIRTIO_MMIO_STATUS, device->status);
    device->status |= VIRTIO_CONFIG_S_DRIVER;
    MMIO_WRITE_UINT_32(device->mmio_base, VIRTIO_MMIO_STATUS, device->status);

    // check available features
    uint64_t offered = virtio_read_features(device->mmio_base);
    if (!(offered & VIRTIO_FEATURE(VIRTIO_F_VERSION_1)))
    {
        virtio_device_fail(device);
        return false;
    }
    device->features =
        offered & (supported_features | VIRTIO_FEATURE(VIRTIO_F_VERSION_1));
    virtio_write_features(device->mmio_base, device->features);
    device->status |= VIRTIO_CONFIG_S_FEATURES_OK;
    MMIO_WRITE_UINT_32(device->mmio_base, VIRTIO_MMIO_STATUS, device->status);
    device->status = MMIO_READ_UINT_32(device->mmio_base, VIRTIO_MMIO_STATUS);

    if (!(device->status & VIRTIO_CONFIG_S_FEATURES_OK))
    {
        virtio_device_fail(device);
        return false;
    }

    return true;
}

void virtio_device_finish(struct Virtio_Device *device)
{
    device->status |= VIRTIO_CONFIG_S_DRIVER_OK;
    MMIO_WRITE_UINT_32(device->mmio_base, VIRTIO_MMIO_STATUS, device->status);
}

void virtio_device_fail(struct Virtio_Device *device)
{
    device->status |= VIRTIO_CONFIG_S_FAILED;
    MMIO_WRITE_UINT_32(device->mmio_base, VIRTIO_MMIO_STATUS, device->status);
}

// can only be used with VIRTIO_MMIO_QUEUE_DESC_LOW, VIRTIO_MMIO_DRIVER_DESC_LOW
// and VIRTIO_MMIO_DEVICE_DESC_LOW
static inline void virt_mmio_write_uint64(size_t base, size_t reg_low,
                                          uint64_t value)
{
    uint32_t low = (uint32_t)value;
    uint32_t high = (uint32_t)(value >> 32);

    MMIO_WRITE_UINT_32(base, reg_low, low);
    MMIO_WRITE_UINT_32(base, reg_low + 4, high);
}

bool virtio_queue_init(struct Virtio_Device *device, struct Virtio_Queue *queue,
                       uint16_t number)
{
    memset(queue, 0, sizeof(*queue));

    queue->device = device;
    queue->number = number;

    size_t base = device->mmio_base;
    MMIO_WRITE_UINT_32(base, VIRTIO_MMIO_QUEUE_SEL, number);

    if (MMIO_READ_UINT_32(base, VIRTIO_MMIO_QUEUE_READY) != 0 ||
        MMIO_READ_UINT_32(base, VIRTIO_MMIO_QUEUE_NUM_MAX) < VIRTIO_DESCRIPTORS)
    {
        return false;
    }

    queue->desc = alloc_page(ALLOC_FLAG_ZERO_MEMORY);
    queue->avail = alloc_page(ALLOC_FLAG_ZERO_MEMORY);
    queue->used = alloc_page(ALLOC_FLAG_ZERO_MEMORY);
    if (!queue->desc || !queue->avail || !queue->used)
    {
        virtio_queue_destroy(queue);
        return false;
    }

    MMIO_WRITE_UINT_32(base, VIRTIO_MMIO_QUEUE_NUM, VIRTIO_DESCRIPTORS);

    uint64_t address = virt_to_phys((size_t)queue->desc);
    virt_mmio_write_uint64(base, VIRTIO_MMIO_QUEUE_DESC_LOW, address);

    address = virt_to_phys((size_t)queue->avail);
    virt_mmio_write_uint64(base, VIRTIO_MMIO_DRIVER_DESC_LOW, address);

    address = virt_to_phys((size_t)queue->used);
    virt_mmio_write_uint64(base, VIRTIO_MMIO_DEVICE_DESC_LOW, address);

    MMIO_WRITE_UINT_32(base, VIRTIO_MMIO_QUEUE_READY, 1);
    for (size_t i = 0; i < VIRTIO_DESCRIPTORS; ++i) queue->free[i] = 1;
    return true;
}

void virtio_queue_destroy(struct Virtio_Queue *queue)
{
    if (queue->desc) free_page(queue->desc);
    if (queue->avail) free_page(queue->avail);
    if (queue->used) free_page(queue->used);
    queue->desc = NULL;
    queue->avail = NULL;
    queue->used = NULL;
}

int32_t virtio_queue_alloc_desc(struct Virtio_Queue *queue)
{
    for (size_t i = 0; i < VIRTIO_DESCRIPTORS; ++i)
    {
        if (queue->free[i])
        {
            queue->free[i] = 0;
            return (int32_t)i;
        }
    }
    return -1;
}

void virtio_queue_free_desc(struct Virtio_Queue *queue, int32_t index)
{
    if ((index < 0) || (index >= VIRTIO_DESCRIPTORS) ||
        (queue->free[index] != 0))
    {
        panic("virtio_queue_free_desc: invalid descriptor");
    }

    memset(&queue->desc[index], 0, sizeof(queue->desc[index]));
    queue->free[index] = 1;

    wakeup(&queue->free[0]);
}

void virtio_queue_free_chain(struct Virtio_Queue *queue, int32_t index)
{
    while (true)
    {
        uint16_t flags = queue->desc[index].flags;
        uint16_t next = queue->desc[index].next;
        virtio_queue_free_desc(queue, index);
        if (!(flags & VRING_DESC_F_NEXT)) break;
        index = next;
    }
}

void virtio_queue_notify(struct Virtio_Queue *queue)
{
    MMIO_WRITE_UINT_32(queue->device->mmio_base, VIRTIO_MMIO_QUEUE_NOTIFY,
                       queue->number);
}

void virtio_queue_submit(struct Virtio_Queue *queue, uint16_t head)
{
    queue->avail->ring[queue->avail->idx % VIRTIO_DESCRIPTORS] = head;
    atomic_thread_fence(memory_order_seq_cst);
    queue->avail->idx++;
    atomic_thread_fence(memory_order_seq_cst);
    virtio_queue_notify(queue);
}

bool virtio_queue_pop_used(struct Virtio_Queue *queue,
                           struct virtq_used_elem *element)
{
    if (queue->used_idx == queue->used->idx)
    {
        return false;
    }

    atomic_thread_fence(memory_order_seq_cst);
    *element = queue->used->ring[queue->used_idx % VIRTIO_DESCRIPTORS];
    queue->used_idx++;
    return true;
}

uint32_t virtio_device_ack_interrupt(struct Virtio_Device *device)
{
    uint32_t status =
        MMIO_READ_UINT_32(device->mmio_base, VIRTIO_MMIO_INTERRUPT_STATUS);
    status &= 0x3;

    MMIO_WRITE_UINT_32(device->mmio_base, VIRTIO_MMIO_INTERRUPT_ACK, status);
    return status;
}
