/* SPDX-License-Identifier: MIT */

#include "libvimixfs.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int get_min_fs_size_in_blocks()
{
    // min fs
    // 1 boot block
    // 1 super block
    // 5 log blocks
    // 1 inode block -> 8 inodes, 7 free
    // 1 bitmap block -> 8192 blocks
    // data blocks:
    // 1 = / dir
    // 1 free block for one tiny file
    // total: 11 blocks;
    return 16;
}

int get_log_size(size_t fs_size_in_blocks)
{
    if (fs_size_in_blocks <= 32)
    {
        return 5;  // impractical, but just for fun
    }
    else if (fs_size_in_blocks <= 128)
    {
        return 16;
    }

    return 32;
}

int get_inode_blocks(size_t fs_size_in_blocks)
{
    size_t ninodes = fs_size_in_blocks / (VIMIXFS_INODES_PER_BLOCK * 8);

    if (ninodes < 4)
    {
        ninodes++;  // a bit extra for tiny fs
    }
    if (ninodes > 0x10000 / VIMIXFS_INODES_PER_BLOCK)
    {
        // limit as the inode number is uint16_t
        ninodes = 0x10000 / VIMIXFS_INODES_PER_BLOCK;
    }
    return ninodes;
}

bool vimixfs_create(struct vimixfs *vifs, const char *filename,
                    size_t fs_size_in_blocks)
{
    if (fs_size_in_blocks < get_min_fs_size_in_blocks())
    {
        fprintf(stderr, "ERROR: min file system size is %d blocks\n",
                get_min_fs_size_in_blocks());
        return false;
    }

    int nlog = get_log_size(fs_size_in_blocks);
    int ninodeblocks = get_inode_blocks(fs_size_in_blocks);
    int ninodes = ninodeblocks * VIMIXFS_INODES_PER_BLOCK;

    int nbitmap = VIMIXFS_BLOCKS_FOR_BITMAP(fs_size_in_blocks);

    // block 0 is reserved (for a boot block)
    // block 1 is super block
    int nmeta = 2 + nlog + ninodeblocks + nbitmap;

    // open and/or create output file
    vifs->fd = open(filename, O_RDWR | O_CREAT | O_TRUNC, 0666);
    if (vifs->fd < 0)
    {
        fprintf(stderr, "ERROR: could not open file %s (error: %s)\n", filename,
                strerror(errno));
        return false;
    }

    // 1 fs block = 1 disk sector
    int nblocks = fs_size_in_blocks - nmeta;

    vifs->super_block.magic = VIMIXFS_MAGIC;
    vifs->super_block.size = fs_size_in_blocks;
    vifs->super_block.nblocks = nblocks;
    vifs->super_block.ninodes = ninodes;
    vifs->super_block.nlog = nlog;
    vifs->super_block.logstart = 2;
    vifs->super_block.inodestart = 2 + nlog;
    vifs->super_block.bmapstart = 2 + nlog + ninodeblocks;

    printf(
        "nmeta %d (boot, super, log blocks %u inode blocks %u, bitmap "
        "blocks "
        "%u) blocks %d total %zd\n",
        nmeta, nlog, ninodeblocks, nbitmap, nblocks, fs_size_in_blocks);

    vifs->freeblock = nmeta;  // the first free block that we can allocate
    vifs->freeinode = 1;      // inode 0 is not used

    char block_buffer[BLOCK_SIZE];
    memset(block_buffer, 0, sizeof(block_buffer));

    // fill file with zeroes
    for (int i = 0; i < fs_size_in_blocks; i++)
    {
        vimixfs_write_sector(vifs, i, block_buffer);
    }

    memmove(block_buffer, &vifs->super_block, sizeof(vifs->super_block));
    vimixfs_write_sector(vifs, VIMIXFS_SUPER_BLOCK_NUMBER, block_buffer);

    return true;
}

bool vimixfs_open(struct vimixfs *vifs, const char *filename)
{
    vifs->fd = open(filename, O_RDWR);
    if (vifs->fd < 0)
    {
        return false;
    }

    char block_buffer[BLOCK_SIZE];
    vimixfs_read_sector(vifs, VIMIXFS_SUPER_BLOCK_NUMBER, block_buffer);
    memmove(&vifs->super_block, block_buffer, sizeof(vifs->super_block));

    return true;
}

void vimixfs_close(struct vimixfs *vifs)
{
    if (vifs->fd >= 0)
    {
        close(vifs->fd);
        vifs->fd = -1;
    }
}

bool vimixfs_read_sector(struct vimixfs *vifs, uint32_t sec, void *buf)
{
    if (sec >= vifs->super_block.size)
    {
        printf("vimixfs_read_sector: out of range\n");
        return false;
    }
    if (lseek(vifs->fd, sec * BLOCK_SIZE, 0) != sec * BLOCK_SIZE)
    {
        printf("error: %s\n", strerror(errno));
        return false;
    }
    ssize_t bytes_read = read(vifs->fd, buf, BLOCK_SIZE);
    if (bytes_read == 0)
    {
        printf("error: unexpected end of file\n");
        return false;
    }
    if (bytes_read != BLOCK_SIZE)
    {
        printf("error: %s\n", strerror(errno));
        return false;
    }
    return true;
}

bool vimixfs_write_sector(struct vimixfs *vifs, uint32_t sec, void *buf)
{
    if (lseek(vifs->fd, sec * BLOCK_SIZE, SEEK_SET) < 0)
    {
        fprintf(stderr, "ERROR: seek failed in write_sector(), %s\n",
                strerror(errno));
        return false;
    }
    ssize_t written = write(vifs->fd, buf, BLOCK_SIZE);
    if (written != BLOCK_SIZE)
    {
        fprintf(stderr, "ERROR: write failed in write_sector(), %s\n",
                strerror(errno));
        return false;
    }
    return true;
}

uint32_t get_next_free_block(struct vimixfs *vifs)
{
    if (vifs->freeblock >= vifs->super_block.size)
    {
        return INVALID_BLOCK_INDEX;  // no more free blocks
    }
    return vifs->freeblock++;
}

uint32_t vimixfs_get_free_block_count(struct vimixfs *vifs)
{
    return vifs->super_block.size - vifs->freeblock;
}

void vimixfs_write_bitmap(struct vimixfs *vifs)
{
    printf("block_alloc_init: first %d blocks have been allocated\n",
           vifs->freeblock);

    uint32_t used = vifs->freeblock;
    size_t bitmap_blocks = VIMIXFS_BLOCKS_FOR_BITMAP(vifs->super_block.size);

    uint8_t buf[BLOCK_SIZE];

    for (size_t b = 0; b < bitmap_blocks; ++b)
    {
        size_t last_index = (b + 1) * (BLOCK_SIZE * 8) - 1;
        size_t first_index = b * (BLOCK_SIZE * 8);
        if (used >= last_index)
        {
            // fully used bitmap block
            memset(&buf, 0xFF, BLOCK_SIZE);
        }
        else if (used < first_index)
        {
            // fully unused bitmap block
            memset(&buf, 0, BLOCK_SIZE);
        }
        else
        {
            // some blocks in this bitmap block are used
            memset(&buf, 0, BLOCK_SIZE);
            uint32_t used_in_block = used - first_index;
            for (int i = 0; i < used_in_block; i++)
            {
                buf[i / 8] = buf[i / 8] | (0x01 << (i % 8));
            }
        }
        vimixfs_write_sector(vifs, vifs->super_block.bmapstart + b, buf);
    }
}