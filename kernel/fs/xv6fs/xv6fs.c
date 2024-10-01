/* SPDX-License-Identifier: MIT */

#include "xv6fs.h"

#include <fs/xv6fs/log.h>
#include <kernel/bio.h>
#include <kernel/buf.h>
#include <kernel/major.h>
#include <kernel/string.h>

/// there should be one xv6fs_superblock per disk device, but we run with
/// only one device
struct xv6fs_superblock sb;

xv6fs_file_type imode_to_xv6_file_type(mode_t imode)
{
    if (S_ISREG(imode)) return XV6_FT_FILE;
    if (S_ISDIR(imode)) return XV6_FT_DIR;
    if (S_ISCHR(imode)) return XV6_FT_CHAR_DEVICE;
    if (S_ISBLK(imode)) return XV6_FT_BLOCK_DEVICE;

    return XV6_FT_UNUSED;
}

mode_t xv6_file_type_to_imode(xv6fs_file_type type)
{
    if (type == XV6_FT_FILE) return S_IFREG | DEFAULT_ACCESS_MODES;
    if (type == XV6_FT_DIR) return S_IFDIR | DEFAULT_ACCESS_MODES;
    if (type == XV6_FT_CHAR_DEVICE) return S_IFCHR | DEFAULT_ACCESS_MODES;
    if (type == XV6_FT_BLOCK_DEVICE) return S_IFBLK | DEFAULT_ACCESS_MODES;

    return 0;
}

struct inode *xv6fs_iops_alloc(dev_t dev, mode_t mode)
{
    for (size_t inum = 1; inum < sb.ninodes; inum++)
    {
        struct buf *bp = bio_read(dev, XV6FS_BLOCK_OF_INODE(inum, sb));
        struct xv6fs_dinode *dip =
            (struct xv6fs_dinode *)bp->data + inum % XV6FS_INODES_PER_BLOCK;

        if (dip->type == XV6_FT_UNUSED)
        {
            // a free inode
            memset(dip, 0, sizeof(*dip));
            dip->type = imode_to_xv6_file_type(mode);
            dip->major = 0;
            dip->minor = 0;
            log_write(bp);  // mark it allocated on the disk
            bio_release(bp);
            return iget(dev, inum);
        }
        bio_release(bp);
    }

    printk("xv6fs_iops_alloc: no inodes\n");
    return NULL;
}

int xv6fs_iops_update(struct inode *ip)
{
    struct buf *bp = bio_read(ip->i_sb_dev, XV6FS_BLOCK_OF_INODE(ip->inum, sb));
    struct xv6fs_dinode *dip =
        (struct xv6fs_dinode *)bp->data + ip->inum % XV6FS_INODES_PER_BLOCK;
    dip->type = imode_to_xv6_file_type(ip->i_mode);

    if (ip->dev == ip->i_sb_dev)
    {
        // map whatever device the filesystem is on to 0
        dip->major = 0;
        dip->minor = 0;
    }
    else
    {
        dip->major = MAJOR(ip->dev);
        dip->minor = MINOR(ip->dev);
    }

    dip->nlink = ip->nlink;
    dip->size = ip->size;
    memmove(dip->addrs, ip->addrs, sizeof(ip->addrs));
    log_write(bp);
    bio_release(bp);

    return 0;
}

int xv6fs_iops_read_in(struct inode *ip)
{
    struct buf *bp = bio_read(ip->i_sb_dev, XV6FS_BLOCK_OF_INODE(ip->inum, sb));
    struct xv6fs_dinode *dip =
        (struct xv6fs_dinode *)bp->data + ip->inum % XV6FS_INODES_PER_BLOCK;
    ip->i_mode = xv6_file_type_to_imode(dip->type);

    if (dip->major == 0 && dip->minor == 0)
    {
        ip->dev = ip->i_sb_dev;  // unmap device 0 to whatever device the
                                 // filesystem is on
    }
    else
    {
        ip->dev = MKDEV(dip->major, dip->minor);
    }

    ip->nlink = dip->nlink;
    ip->size = dip->size;
    memmove(ip->addrs, dip->addrs, sizeof(ip->addrs));
    bio_release(bp);

    return 0;
}

int xv6fs_iops_trunc(struct inode *ip)
{
    for (size_t i = 0; i < XV6FS_N_DIRECT_BLOCKS; i++)
    {
        if (ip->addrs[i])
        {
            bfree(ip->dev, ip->addrs[i]);
            ip->addrs[i] = 0;
        }
    }

    if (ip->addrs[XV6FS_N_DIRECT_BLOCKS])
    {
        struct buf *bp = bio_read(ip->dev, ip->addrs[XV6FS_N_DIRECT_BLOCKS]);
        uint32_t *a = (uint32_t *)bp->data;
        for (size_t j = 0; j < XV6FS_N_INDIRECT_BLOCKS; j++)
        {
            if (a[j])
            {
                bfree(ip->dev, a[j]);
            }
        }
        bio_release(bp);
        bfree(ip->dev, ip->addrs[XV6FS_N_DIRECT_BLOCKS]);
        ip->addrs[XV6FS_N_DIRECT_BLOCKS] = 0;
    }

    return 0;
}

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
    for (uint32_t b = 0; b < sb.size; b += XV6FS_BMAP_BITS_PER_BLOCK)
    {
        struct buf *bp =
            bio_read(dev, XV6FS_BMAP_BLOCK_OF_BIT(b, sb.bmapstart));
        for (uint32_t bi = 0;
             bi < XV6FS_BMAP_BITS_PER_BLOCK && b + bi < sb.size; bi++)
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
    struct buf *bp = bio_read(dev, XV6FS_BMAP_BLOCK_OF_BIT(b, sb.bmapstart));
    int32_t bi = b % XV6FS_BMAP_BITS_PER_BLOCK;
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
    if (bn < XV6FS_N_DIRECT_BLOCKS)
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
    bn -= XV6FS_N_DIRECT_BLOCKS;

    if (bn < XV6FS_N_INDIRECT_BLOCKS)
    {
        // Load indirect block, allocating if necessary.
        size_t addr = ip->addrs[XV6FS_N_DIRECT_BLOCKS];
        if (addr == 0)
        {
            addr = balloc(ip->dev);
            if (addr == 0)
            {
                return 0;
            }
            ip->addrs[XV6FS_N_DIRECT_BLOCKS] = addr;
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
    return 0;
}
