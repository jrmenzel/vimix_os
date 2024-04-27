/* SPDX-License-Identifier: MIT */

#include <drivers/virtio_disk.h>
#include <kernel/buf.h>
#include <kernel/fs.h>
#include <kernel/kernel.h>
#include <kernel/printk.h>
#include <kernel/sleeplock.h>
#include <kernel/spinlock.h>

struct
{
    struct spinlock lock;
    struct buf buf[NUM_BUFFERS_IN_CACHE];

    // Linked list of all buffers, through prev/next.
    // Sorted by how recently the buffer was used.
    // head.next is most recent, head.prev is least.
    struct buf head;
} g_buf_cache;

void bio_init()
{
    struct buf *b;

    spin_lock_init(&g_buf_cache.lock, "g_buf_cache");

    // Create linked list of buffers
    g_buf_cache.head.prev = &g_buf_cache.head;
    g_buf_cache.head.next = &g_buf_cache.head;
    for (b = g_buf_cache.buf; b < g_buf_cache.buf + NUM_BUFFERS_IN_CACHE; b++)
    {
        b->next = g_buf_cache.head.next;
        b->prev = &g_buf_cache.head;
        sleep_lock_init(&b->lock, "buffer");
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
    struct buf *b;

    spin_lock(&g_buf_cache.lock);

    // Is the block already cached?
    for (b = g_buf_cache.head.next; b != &g_buf_cache.head; b = b->next)
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
    for (b = g_buf_cache.head.prev; b != &g_buf_cache.head; b = b->prev)
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
}

struct buf *bio_read(uint32_t dev, uint32_t blockno)
{
    struct buf *b;

    b = bget(dev, blockno);
    if (!b->valid)
    {
        virtio_disk_rw(b, 0);
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

    virtio_disk_rw(b, 1);
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
