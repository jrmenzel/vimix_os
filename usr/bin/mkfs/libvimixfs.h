/* SPDX-License-Identifier: MIT */
#pragma once

#include <kernel/vimixfs.h>
#include <stdint.h>

struct vimixfs
{
    int fd;

    // super block of opened file system:
    struct vimixfs_superblock super_block;

    // log header of the opened file system:
    struct vimixfs_log_header log_header;

    // next free inode number (assuming no inodes get deleted)
    ino_t freeinode;

    // next free block number (assuming no blocks get deleted)
    uint32_t freeblock;

    // to build a bitmap of used blocks and compare it to the bitmap of the fs
    // later:
    uint8_t *bitmap;
    size_t bitmap_errors;

    // store for each inode if it is in use and if it is referenced by a
    // directory:
#define INODE_UNUSED 0
#define INODE_REFERENCED 1
#define INODE_DEFINE 2
#define INODE_OK(x) \
    ((x == INODE_UNUSED) || (x == (INODE_REFERENCED & INODE_DEFINE)))
    uint8_t *inodes;
    size_t inode_errors;
};

#define INVALID_BLOCK_INDEX 0xFFFFFFFF

bool vimixfs_create(struct vimixfs *vifs, const char *filename,
                    size_t fs_size_in_blocks);

bool vimixfs_open(struct vimixfs *vifs, const char *filename);

void vimixfs_close(struct vimixfs *vifs);

bool vimixfs_read_sector(struct vimixfs *vifs, uint32_t sec, void *buf);

bool vimixfs_write_sector(struct vimixfs *vifs, uint32_t sec, void *buf);

uint32_t get_next_free_block(struct vimixfs *vifs);

uint32_t vimixfs_get_free_block_count(struct vimixfs *vifs);

void vimixfs_write_bitmap(struct vimixfs *vifs);
