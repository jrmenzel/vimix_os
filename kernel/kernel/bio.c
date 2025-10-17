/* SPDX-License-Identifier: MIT */

#include <drivers/block_device.h>
#include <kernel/bio.h>
#include <kernel/bio_sysfs.h>
#include <kernel/buf.h>
#include <kernel/fs.h>
#include <kernel/kernel.h>
#include <kernel/sleeplock.h>
#include <mm/kalloc.h>
#include <mm/kernel_memory.h>

struct bio_cache g_buf_cache;

void bio_init()
{
    spin_lock_init(&g_buf_cache.lock, "g_buf_cache");
    kobject_init(&g_buf_cache.kobj, &bio_kobj_ktype);
    kobject_add(&g_buf_cache.kobj, &g_kernel_memory.kobj, "bio");

    list_init(&g_buf_cache.buf_list);

    g_buf_cache.num_buffers = 0;
    g_buf_cache.free_buffers = 0;
    g_buf_cache.max_free_buffers = 16;  // arbitrary default
    spin_lock(&g_buf_cache.lock);       // the setter tests for the lock
    bio_cache_set_min_buffers(&g_buf_cache, 16);  // arbitrary default
    spin_unlock(&g_buf_cache.lock);
}

/// Look through buffer cache for the requested block on device dev.
/// If the block was cached, increase the ref count and return.
/// If not found, allocate a buffer.
/// In either case, return a locked buffer.
/// Buffer content is not zeroed.
struct buf *bio_get_from_cache(dev_t dev, uint32_t blockno)
{
    spin_lock(&g_buf_cache.lock);

    struct buf *free_buffer = NULL;
    struct buf *buffer = NULL;

    // Is the block already cached?
    // While looking, remember the first unused buffer.
    struct list_head *pos;
    list_for_each(pos, &g_buf_cache.buf_list)
    {
        struct buf *b = buf_from_list(pos);

        if (b->dev == dev && b->blockno == blockno)
        {
            if (b->refcnt == 0)
            {
                g_buf_cache.free_buffers--;
            }
            b->refcnt++;
            buffer = b;
            break;
        }
        else if ((free_buffer == NULL) && (b->refcnt == 0))
        {
            // first unused buffer in the list is the oldest one
            free_buffer = b;
        }
    }

    if (buffer == NULL)
    {
        // Not cached...
        if (free_buffer == NULL)
        {
            // ...and no free buffer found, allocate a new one
            buffer = buf_alloc_init(dev, blockno);
        }
        else
        {
            // ...but we found a free buffer, reuse it
            buffer = free_buffer;
            buf_reinit(buffer, dev, blockno);
            g_buf_cache.free_buffers--;
        }
    }

    spin_unlock(&g_buf_cache.lock);
    sleep_lock(&buffer->lock);

    return buffer;
}

/// Return a locked buf with the contents of the indicated block.
struct buf *bio_read(dev_t dev, uint32_t blockno)
{
    struct buf *b = bio_get_from_cache(dev, blockno);

    if (b->valid == false)
    {
        struct Block_Device *bdevice = get_block_device(dev);
        if (!bdevice)
        {
            panic("bio_read called for non block device!");
        }

        bdevice->ops.read_buf(bdevice, b);

        b->valid = true;
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

bool bio_has_too_many_buffers()
{
    if (g_buf_cache.num_buffers <= g_buf_cache.min_buffers)
    {
        // keep a minimum amount of buffers
        return false;
    }
    else if (g_buf_cache.free_buffers <= g_buf_cache.max_free_buffers)
    {
        // keep a few free buffers to reduce kmalloc/free calls
        return false;
    }
    return true;
}

void bio_might_free(struct buf *b)
{
    if (bio_has_too_many_buffers())
    {
        // free buffer
        buf_deinit(b);
        kfree(b);
    }
    else
    {
        // move to the end of the list, so it can be reused later
        list_del(&b->buf_list);
        list_add_tail(&b->buf_list, &g_buf_cache.buf_list);
        g_buf_cache.free_buffers++;
    }
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
        bio_might_free(b);
    }

    spin_unlock(&g_buf_cache.lock);
}

void bio_get(struct buf *b)
{
    spin_lock(&g_buf_cache.lock);
    b->refcnt++;
    spin_unlock(&g_buf_cache.lock);
}

void bio_put(struct buf *b)
{
    spin_lock(&g_buf_cache.lock);
    b->refcnt--;
    spin_unlock(&g_buf_cache.lock);
}

void bio_cache_free_extra_buffers(struct bio_cache *cache)
{
    DEBUG_EXTRA_PANIC(spin_lock_is_held_by_this_cpu(&cache->lock),
                      "bio_cache_free_extra_buffers: lock not held");

    struct list_head *pos, *n;
    list_for_each_safe(pos, n, &cache->buf_list)
    {
        struct buf *b = buf_from_list(pos);
        if ((b->refcnt == 0) && (bio_has_too_many_buffers()))
        {
            g_buf_cache.free_buffers--;
            buf_deinit(b);
            kfree(b);
        }
    }
}

ssize_t bio_cache_set_min_buffers(struct bio_cache *cache, ssize_t min_buffers)
{
    DEBUG_EXTRA_PANIC(spin_lock_is_held_by_this_cpu(&cache->lock),
                      "bio_cache_set_min_buffers: lock not held");
    if (min_buffers < 3) return -1;  // one page worth of buffers

    cache->min_buffers = (size_t)min_buffers;

    // allocate new buffers if needed
    for (size_t i = cache->num_buffers; i < g_buf_cache.min_buffers; i++)
    {
        struct buf *b = buf_alloc_init(0, 0);
        if (b == NULL)
        {
            panic("bio_init: buf_alloc_init failed");
        }
        b->refcnt = 0;  // drop the implicit reference from buf_alloc_init()
        g_buf_cache.free_buffers++;
    }
    bio_cache_free_extra_buffers(cache);

    return 0;
}

ssize_t bio_cache_set_max_free_buffers(struct bio_cache *cache,
                                       ssize_t max_free_buffers)
{
    DEBUG_EXTRA_PANIC(spin_lock_is_held_by_this_cpu(&cache->lock),
                      "bio_cache_set_max_free_buffers: lock not held");
    if (max_free_buffers < 0) return -1;

    cache->max_free_buffers = (size_t)max_free_buffers;
    bio_cache_free_extra_buffers(cache);

    return 0;
}