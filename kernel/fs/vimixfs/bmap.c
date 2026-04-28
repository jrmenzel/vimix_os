/* SPDX-License-Identifier: MIT */

#include <fs/vimixfs/bmap.h>
#include <fs/vimixfs/vimixfs.h>
#include <kernel/bio.h>
#include <kernel/kernel.h>
#include <kernel/string.h>

/// Zero a block.
static void block_zero(dev_t dev, struct log *log, uint32_t blockno)
{
    struct buf *bp = bio_read(dev, blockno);
    memset(bp->data, 0, BLOCK_SIZE);
    log_write(log, bp);
    bio_release(bp);
}

uint32_t block_alloc_init(struct super_block *sb)
{
    struct vimixfs_sb_private *priv =
        (struct vimixfs_sb_private *)sb->s_fs_info;
    struct vimixfs_superblock *vsb = &(priv->sb);

    for (uint32_t b = 0; b < vsb->size; b += VIMIXFS_BMAP_BITS_PER_BLOCK)
    {
        struct buf *bp =
            bio_read(sb->dev, VIMIXFS_BMAP_BLOCK_OF_BIT(b, vsb->bmapstart));
        for (uint32_t bi = 0;
             bi < VIMIXFS_BMAP_BITS_PER_BLOCK && b + bi < vsb->size; bi++)
        {
            uint32_t m = 1 << (bi % 8);
            if ((bp->data[bi / 8] & m) == 0)
            {                           // Is block free?
                bp->data[bi / 8] |= m;  // Mark block in use.
                log_write(&(priv->log), bp);
                bio_release(bp);
                block_zero(sb->dev, &(priv->log), b + bi);
                return b + bi;
            }
        }
        bio_release(bp);
    }

    return 0;
}

/// Free a disk block.
void block_free(struct super_block *sb, uint32_t block_id)
{
    struct vimixfs_sb_private *priv =
        (struct vimixfs_sb_private *)sb->s_fs_info;
    struct vimixfs_superblock *xsb = &(priv->sb);

    // the block containing the bit for block_id
    struct buf *local_bitmap =
        bio_read(sb->dev, VIMIXFS_BMAP_BLOCK_OF_BIT(block_id, xsb->bmapstart));

    // id inside of that block
    int32_t local_id = block_id % VIMIXFS_BMAP_BITS_PER_BLOCK;
    size_t local_byte = local_id / 8;
    int32_t bit_in_byte = 1 << (local_id % 8);

    DEBUG_EXTRA_PANIC(((local_bitmap->data[local_byte] & bit_in_byte) != 0),
                      "block_free: freeing already free block");

    // clear bit
    local_bitmap->data[local_byte] &= ~bit_in_byte;

    log_write(&(priv->log), local_bitmap);
    bio_release(local_bitmap);
}

uint32_t bmap_from_block_range(struct inode *ip, uint32_t *addr_block,
                               size_t block_number, bool *did_allocate)
{
    uint32_t addr = addr_block[block_number];
    if (addr == 0)
    {
        addr = block_alloc_init(ip->i_sb);
        if (addr != 0)
        {
            addr_block[block_number] = addr;
            if (did_allocate != NULL)
            {
                *did_allocate = true;
            }
        }
    }
    return addr;
}

uint32_t bmap_from_block(struct inode *ip, uint32_t ib_addr,
                         uint32_t block_number)
{
    if (ib_addr == 0)
    {
        return 0;
    }

    // Load indirect block
    struct buf *bp = bio_read(ip->dev, ib_addr);
    uint32_t *indirect_block = (uint32_t *)bp->data;

    bool did_allocate = false;
    uint32_t addr =
        bmap_from_block_range(ip, indirect_block, block_number, &did_allocate);
    if (did_allocate)
    {
        struct vimixfs_sb_private *priv =
            (struct vimixfs_sb_private *)ip->i_sb->s_fs_info;
        log_write(&(priv->log), bp);
    }

    bio_release(bp);
    return addr;
}

/// Return the disk block address of the nth block in inode ip.
/// If there is no such block, bmap_get_block_address allocates one.
/// returns 0 if out of disk space.
size_t bmap_get_block_address(struct inode *ip, uint32_t block_number)
{
    struct vimixfs_inode *xv_ip = vimixfs_inode_from_inode(ip);
    if (block_number < VIMIXFS_N_DIRECT_BLOCKS)
    {
        return bmap_from_block_range(ip, xv_ip->addrs, block_number, NULL);
    }
    block_number -= VIMIXFS_N_DIRECT_BLOCKS;

    if (block_number < VIMIXFS_N_INDIRECT_BLOCKS)
    {
        if (xv_ip->addrs[VIMIXFS_INDIRECT_BLOCK_IDX] == 0)
        {
            // allocate indirect block
            xv_ip->addrs[VIMIXFS_INDIRECT_BLOCK_IDX] =
                block_alloc_init(ip->i_sb);
        }
        return bmap_from_block(ip, xv_ip->addrs[VIMIXFS_INDIRECT_BLOCK_IDX],
                               block_number);
    }
    block_number -= VIMIXFS_N_INDIRECT_BLOCKS;

    if (block_number < VIMIXFS_N_INDIRECT_BLOCKS * VIMIXFS_N_INDIRECT_BLOCKS)
    {
        if (xv_ip->addrs[VIMIXFS_DOUBLE_INDIRECT_BLOCK_IDX] == 0)
        {
            // allocate double indirect block
            xv_ip->addrs[VIMIXFS_DOUBLE_INDIRECT_BLOCK_IDX] =
                block_alloc_init(ip->i_sb);
        }
        size_t index_0 = block_number / VIMIXFS_N_INDIRECT_BLOCKS;
        size_t index_1 = block_number % VIMIXFS_N_INDIRECT_BLOCKS;
        uint32_t indirect_block = bmap_from_block(
            ip, xv_ip->addrs[VIMIXFS_DOUBLE_INDIRECT_BLOCK_IDX], index_0);
        return bmap_from_block(ip, indirect_block, index_1);
    }

    panic("bmap_get_block_address: out of range");
    return 0;
}
