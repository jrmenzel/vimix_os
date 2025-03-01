/* SPDX-License-Identifier: MIT */

#include <drivers/block_device.h>
#include <kernel/bio.h>
#include <kernel/buf.h>
#include <kernel/fs.h>
#include <kernel/kernel.h>
#include <kernel/sleeplock.h>
#include <kernel/spinlock.h>

/// @brief Main buffer cache.
struct
{
    struct spinlock lock;

    // buffers to be used, access via head and the next/prev pointers
    struct buf buf[NUM_BUFFERS_IN_CACHE];

    // Linked list of all buffers, through prev/next.
    // Sorted by how recently the buffer was used.
    // head.next is most recent, head.prev is least.
    struct buf head;
} g_buf_cache;

void bio_init()
{
    spin_lock_init(&g_buf_cache.lock, "g_buf_cache");

    // Create linked list of buffers
    g_buf_cache.head.prev = &g_buf_cache.head;
    g_buf_cache.head.next = &g_buf_cache.head;
    for (struct buf *b = g_buf_cache.buf;
         b < g_buf_cache.buf + NUM_BUFFERS_IN_CACHE; b++)
    {
        // init buffer
        sleep_lock_init(&b->lock, "buffer");
        b->valid = 0;

        // setup linked list
        b->next = g_buf_cache.head.next;
        b->prev = &g_buf_cache.head;
        g_buf_cache.head.next->prev = b;
        g_buf_cache.head.next = b;
    }
}

/// Look through buffer cache for the requested block on device dev.
/// If the block was cached, increase the ref count and return.
/// If not found, allocate a buffer.
/// In either case, return a locked buffer.
static struct buf *bget(dev_t dev, uint32_t blockno)
{
    spin_lock(&g_buf_cache.lock);

    // Is the block already cached?
    for (struct buf *b = g_buf_cache.head.next; b != &g_buf_cache.head;
         b = b->next)
    {
        if (b->dev == dev && b->blockno == blockno)
        {
            b->refcnt++;
            spin_unlock(&g_buf_cache.lock);
            sleep_lock(&b->lock);
            return b;
        }
    }

    // Not cached.
    // Recycle the least recently used (LRU) unused buffer.
    for (struct buf *b = g_buf_cache.head.prev; b != &g_buf_cache.head;
         b = b->prev)
    {
        if (b->refcnt == 0)
        {
            b->dev = dev;
            b->blockno = blockno;
            b->valid = 0;
            b->refcnt = 1;
            spin_unlock(&g_buf_cache.lock);
            sleep_lock(&b->lock);
            return b;
        }
    }

    panic("bget: no buffers");
    return 0;
}

/// Return a locked buf with the contents of the indicated block.
struct buf *bio_read(dev_t dev, uint32_t blockno)
{
    struct buf *b = bget(dev, blockno);

    if (!b->valid)
    {
        struct Block_Device *bdevice = get_block_device(dev);
        if (!bdevice)
        {
            panic("bio_read called for non block device!");
        }

        bdevice->ops.read_buf(bdevice, b);

        b->valid = 1;
    }
    return b;
}

void bio_write(struct buf *b)
{
#ifdef CONFIG_DEBUG_SLEEPLOCK
    if (!sleep_lock_is_held_by_this_cpu(&b->lock))
    {
        panic("bio_write: not holding the sleeplock");
    }
#endif  // CONFIG_DEBUG_SLEEPLOCK

    struct Block_Device *bdevice = get_block_device(b->dev);
    if (!bdevice)
    {
        panic("bio_write called for non block device!");
    }

    bdevice->ops.write_buf(bdevice, b);
}

void bio_release(struct buf *b)
{
#ifdef CONFIG_DEBUG_SLEEPLOCK
    if (!sleep_lock_is_held_by_this_cpu(&b->lock))
    {
        panic("bio_release: not holding the sleeplock");
    }
#endif  // CONFIG_DEBUG_SLEEPLOCK

    sleep_unlock(&b->lock);

    spin_lock(&g_buf_cache.lock);
    b->refcnt--;
    if (b->refcnt == 0)
    {
        // no one is waiting for it.
        b->next->prev = b->prev;
        b->prev->next = b->next;
        b->next = g_buf_cache.head.next;
        b->prev = &g_buf_cache.head;
        g_buf_cache.head.next->prev = b;
        g_buf_cache.head.next = b;
    }

    spin_unlock(&g_buf_cache.lock);
}

void bio_pin(struct buf *b)
{
    spin_lock(&g_buf_cache.lock);
    b->refcnt++;
    spin_unlock(&g_buf_cache.lock);
}

void bio_unpin(struct buf *b)
{
    spin_lock(&g_buf_cache.lock);
    b->refcnt--;
    spin_unlock(&g_buf_cache.lock);
}
