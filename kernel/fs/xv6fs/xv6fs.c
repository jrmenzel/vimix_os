/* SPDX-License-Identifier: MIT */

#include "xv6fs.h"

#include <fs/xv6fs/log.h>
#include <kernel/bio.h>
#include <kernel/buf.h>
#include <kernel/string.h>

/// there should be one xv6fs_superblock per disk device, but we run with
/// only one device
struct xv6fs_superblock sb;

/// Zero a block.
static void block_zero(dev_t dev, uint32_t blockno)
{
    struct buf *bp = bio_read(dev, blockno);
    memset(bp->data, 0, BLOCK_SIZE);
    log_write(bp);
    bio_release(bp);
}

uint32_t balloc(dev_t dev)
{
    for (uint32_t b = 0; b < sb.size; b += BPB)
    {
        struct buf *bp = bio_read(dev, BBLOCK(b, sb));
        for (uint32_t bi = 0; bi < BPB && b + bi < sb.size; bi++)
        {
            uint32_t m = 1 << (bi % 8);
            if ((bp->data[bi / 8] & m) == 0)
            {                           // Is block free?
                bp->data[bi / 8] |= m;  // Mark block in use.
                log_write(bp);
                bio_release(bp);
                block_zero(dev, b + bi);
                return b + bi;
            }
        }
        bio_release(bp);
    }
    printk("balloc: out of blocks\n");
    return 0;
}

/// Free a disk block.
void bfree(dev_t dev, uint32_t b)
{
    struct buf *bp = bio_read(dev, BBLOCK(b, sb));
    int32_t bi = b % BPB;
    int32_t m = 1 << (bi % 8);

    if ((bp->data[bi / 8] & m) == 0)
    {
        panic("freeing free block");
    }
    bp->data[bi / 8] &= ~m;
    log_write(bp);
    bio_release(bp);
}

size_t bmap(struct inode *ip, uint32_t bn)
{
    if (bn < NDIRECT)
    {
        size_t addr = ip->addrs[bn];
        if (addr == 0)
        {
            addr = balloc(ip->dev);
            if (addr == 0)
            {
                return 0;
            }
            ip->addrs[bn] = addr;
        }
        return addr;
    }
    bn -= NDIRECT;

    if (bn < NINDIRECT)
    {
        // Load indirect block, allocating if necessary.
        size_t addr = ip->addrs[NDIRECT];
        if (addr == 0)
        {
            addr = balloc(ip->dev);
            if (addr == 0)
            {
                return 0;
            }
            ip->addrs[NDIRECT] = addr;
        }
        struct buf *bp = bio_read(ip->dev, addr);
        uint32_t *a = (uint32_t *)bp->data;
        addr = a[bn];
        if (addr == 0)
        {
            addr = balloc(ip->dev);
            if (addr)
            {
                a[bn] = addr;
                log_write(bp);
            }
        }
        bio_release(bp);
        return addr;
    }

    panic("bmap: out of range");
}
