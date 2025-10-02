/* SPDX-License-Identifier: MIT */

#include <kernel/bio.h>
#include <kernel/buf.h>
#include <mm/kalloc.h>

int i = 0;

struct buf *buf_alloc_init(dev_t dev, uint32_t blockno)
{
    struct buf *b = kmalloc(sizeof(struct buf), ALLOC_FLAG_NONE);
    if (b == NULL)
    {
        return NULL;
    }

    buf_init(b, dev, blockno);
    return b;
}

void buf_init(struct buf *b, dev_t dev, uint32_t blockno)
{
    sleep_lock_init(&b->lock, "buffer");
    list_init(&b->buf_list);

    buf_reinit(b, dev, blockno);

    b->id = i++;

    list_add(&b->buf_list, &g_buf_cache.buf_list);
    g_buf_cache.num_buffers++;
}

void buf_reinit(struct buf *b, dev_t dev, uint32_t blockno)
{
    b->dev = dev;
    b->blockno = blockno;
    b->valid = false;
    b->disk = 0;
    b->refcnt = 1;
}

void buf_deinit(struct buf *b)
{
    list_del(&b->buf_list);
    g_buf_cache.num_buffers--;
}