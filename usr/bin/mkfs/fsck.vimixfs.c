/* SPDX-License-Identifier: MIT */

#include <errno.h>
#include <fcntl.h>
#include <kernel/major.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "libvimixfs.h"
#include "libvimixfs_internal.h"

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wanalyzer-use-of-uninitialized-value"
void mark_block_as_used(struct vimixfs* file, uint32_t addr)
{
    if (file == NULL) return;
    if (file->shadow_bitmap == NULL)
    {
        size_t bytes_to_allocate =
            VIMIXFS_BLOCKS_FOR_BITMAP(file->super_block.size) * BLOCK_SIZE;

        file->shadow_bitmap = (uint8_t*)malloc(bytes_to_allocate);
        if (file->shadow_bitmap == NULL)
        {
            fprintf(stderr, "ERROR: out of memory\n");
            exit(1);
        }
        memset(file->shadow_bitmap, 0, bytes_to_allocate);
    }

    // the offset is 0 instead of bmapstart as the bitmap is the array g_bitmap
    // and not the in-file bitmap
    size_t block = VIMIXFS_BMAP_BLOCK_OF_BIT(addr, 0);
    uint8_t* bitmap_block = file->shadow_bitmap + (BLOCK_SIZE * block);
    int32_t local_addr = addr % VIMIXFS_BMAP_BITS_PER_BLOCK;

    uint8_t bit_to_set = 1 << (local_addr % 8);
    local_addr = local_addr / 8;

    if ((bitmap_block[local_addr] & bit_to_set) != 0)
    {
        printf("Error: block is in use multiple times %d\n", addr);
    }

    bitmap_block[local_addr] = bitmap_block[local_addr] | bit_to_set;
}
#pragma GCC diagnostic pop

void print_fs_layout(struct vimixfs* file)
{
    struct vimixfs_superblock* sb = &file->super_block;
    size_t blocks_for_bitmap = VIMIXFS_BLOCKS_FOR_BITMAP(sb->size);
    size_t blocks_for_inodes = sb->ninode_blocks;

    printf("Blocks:\n");
    printf("[              0] = Reserved for boot loader\n");
    printf("[              1] = Superblock\n");
    printf("[         %6d] = Log Header\n", sb->logstart);
    int log_entry_start = sb->logstart + 1;
    int log_entry_end = sb->logstart + sb->nlog - 1;
    printf("[%6d - %6d] = %d Log Entries\n", log_entry_start, log_entry_end,
           sb->nlog - 1);

    printf("[%6d - %6zd] = Inodes (%zd inodes per block)\n", sb->inodestart,
           sb->inodestart + blocks_for_inodes - 1, VIMIXFS_INODES_PER_BLOCK);
    size_t bmap_end = sb->bmapstart + blocks_for_bitmap - 1;
    printf("[%6d - %6zd] = Bitmap\n", sb->bmapstart, bmap_end);
    size_t data_start = bmap_end + 1;
    printf("[%6zd - %6zd] = Data\n", data_start, data_start + sb->nblocks);

    for (uint32_t b = 0; b < data_start; ++b)
    {
        mark_block_as_used(file, b);
    }

    if (sb->size != data_start + sb->nblocks)
    {
        printf("FS size error\n");
    }
}

void print_super_block_info(struct vimixfs* file)
{
    struct vimixfs_superblock* sb = &file->super_block;
    printf("Super Block\n");
    if (sb->magic != VIMIXFS_MAGIC)
    {
        printf("ERROR: invalid magic number %x (expected %x)\n", sb->magic,
               VIMIXFS_MAGIC);
    }
    else
    {
        printf("Magic:        %x\n", sb->magic);
    }
    printf("Size:         %d blocks/kb\n", sb->size);
    printf("Data Blocks:  %d\n", sb->nblocks);
    printf("Max Inodes:   %zd\n", sb->ninode_blocks * VIMIXFS_INODES_PER_BLOCK);
    printf("Log Blocks:   %d\n", sb->nlog);
    printf("Log Start:    %d\n", sb->logstart);
    printf("Inode Start:  %d\n", sb->inodestart);
    printf("Bitmap Start: %d\n", sb->bmapstart);
}

void print_log_header(struct vimixfs* file)
{
    struct vimixfs_log_header* log = &file->log_header;
    if (log->n == 0)
    {
        printf("Log clean\n");
        return;
    }

    printf("Log contains %d entries\n", log->n);
    for (size_t i = 0; i < log->n; ++i)
    {
        printf(" log %zd = block %d\n", i, log->block[i]);
    }
}

void check_dirents(struct vimixfs* file, uint32_t addr)
{
    uint8_t buf[BLOCK_SIZE];
    vimixfs_read_sector(file, addr, buf);
    struct vimixfs_dirent* dir = (struct vimixfs_dirent*)buf;

    size_t dirents_per_block = BLOCK_SIZE % sizeof(struct vimixfs_dirent);
    for (size_t i = 0; i < dirents_per_block; ++i)
    {
        size_t inum = dir[i].inum;
        file->inodes[inum] = file->inodes[inum] & INODE_REFERENCED;
    }
}

void check_block_range(struct vimixfs* file, uint32_t* range, size_t range_len,
                       bool verbose)
{
    for (size_t i = 0; i < range_len; ++i)
    {
        if (range[i] != 0)
        {
            mark_block_as_used(file, range[i]);
            check_dirents(file, range[i]);
            if (verbose) printf(" [%d]", range[i]);
        }
    }
}

// caller must free the returned pointer
uint32_t* read_indirect_block(struct vimixfs* file, uint32_t addr)
{
    if (addr == 0) exit(1);  // should not happen

    mark_block_as_used(file, addr);
    uint8_t* buffer = malloc(BLOCK_SIZE);
    if (buffer == NULL)
    {
        printf("ERROR: out of memory\n");
        exit(1);
    }
    vimixfs_read_sector(file, addr, buffer);
    return (uint32_t*)buffer;
}

void check_indirect_block(struct vimixfs* file, uint32_t addr, bool verbose)
{
    if (addr == 0) return;

    uint32_t* block_addr = read_indirect_block(file, addr);
    check_block_range(file, block_addr, BLOCK_SIZE / sizeof(uint32_t), verbose);
    free((void*)block_addr);
}

void check_double_indirect_block(struct vimixfs* file, uint32_t addr,
                                 bool verbose)
{
    if (addr == 0) return;

    uint32_t* next_block_addr = read_indirect_block(file, addr);
    for (size_t i = 0; i < BLOCK_SIZE / sizeof(uint32_t); ++i)
    {
        check_indirect_block(file, next_block_addr[i], verbose);
    }
    free((void*)next_block_addr);
}

// returns 1 if the disk inode is in use, 0 otherwise
int check_dinode(struct vimixfs* file, struct vimixfs_dinode* dinode,
                 size_t inum, bool verbose)
{
    if (file == NULL || file->inodes == NULL || dinode == NULL)
    {
        printf("ERROR in check_dinode\n");
        exit(1);
    }

    if (dinode->mode == VIMIXFS_INVALID_MODE)
    {
        file->inodes[inum] = INODE_UNUSED;
        return 0;
    }

    if (verbose)
    {
        size_t block_idx = VIMIXFS_BLOCK_IDX_OF_INODE(inum);
        size_t offset = VIMIXFS_DINODE_OFFSET_IN_BLOCK(inum);

        printf("%zd (block %zd, %zd) ", inum, block_idx, offset);
    }

    switch ((dinode->mode) & S_IFMT)
    {
        case S_IFDIR:
            if (verbose) printf("dir");
            break;
        case S_IFREG:
            if (verbose) printf("file");
            break;
        case S_IFCHR:
            if (verbose)
                printf("c dev (%d,%d)", MAJOR(dinode->dev), MINOR(dinode->dev));
            break;
        case S_IFBLK:
            if (verbose)
                printf("b dev (%d,%d)", MAJOR(dinode->dev), MINOR(dinode->dev));
            break;
        default:
            printf("UNKNOWN inode type %d\n", dinode->mode & S_IFMT);
            return 0;
    }
    file->inodes[inum] = file->inodes[inum] & INODE_DEFINE;

    if ((dinode->mode & S_IFDIR) || (dinode->mode & S_IFREG))
    {
        check_block_range(file, dinode->addrs, VIMIXFS_N_DIRECT_BLOCKS,
                          verbose);

        check_indirect_block(file, dinode->addrs[VIMIXFS_INDIRECT_BLOCK_IDX],
                             verbose);

        check_double_indirect_block(
            file, dinode->addrs[VIMIXFS_DOUBLE_INDIRECT_BLOCK_IDX], verbose);
    }

    if (verbose) printf("\n");

    return 1;
}

void check_inodes(struct vimixfs* file, bool verbose)
{
    struct vimixfs_superblock* sb = &file->super_block;
    uint8_t buffer[BLOCK_SIZE];
    size_t used = 0;
    ino_t inum = INVALID_INODE;
    size_t inode_end = sb->inodestart + sb->ninode_blocks;
    for (size_t block = sb->inodestart; block < inode_end; ++block)
    {
        vimixfs_read_sector(file, block, buffer);
        struct vimixfs_dinode* dinode = (struct vimixfs_dinode*)buffer;
        for (size_t i = 0; i < VIMIXFS_INODES_PER_BLOCK; ++i)
        {
            used += check_dinode(file, &dinode[i], inum, verbose);
            inum++;
        }
    }
    printf("%zd / %zd disk inodes used\n", used,
           sb->ninode_blocks * VIMIXFS_INODES_PER_BLOCK);
}

size_t check_bitmap_char(uint8_t bm_file, uint8_t bm_calc, size_t offset)
{
    size_t errors = 0;
    for (size_t i = 0; i < 8; ++i)
    {
        uint8_t bit = 1 << i;
        uint8_t b_file = bm_file & bit;
        uint8_t b_calc = bm_calc & bit;

        if (b_file != b_calc)
        {
            errors++;
            if (b_file)
            {
                printf(
                    "error, block %zd in use in file but not accessable from "
                    "inodes\n",
                    offset + i);
            }
            else
            {
                printf("error, block %zd in use from inodes but free in file\n",
                       offset + i);
            }
        }
    }
    return errors;
}

void check_bitmap(struct vimixfs* file)
{
    if (file->shadow_bitmap == NULL)
    {
        return;
    }

    size_t bitmap_size =
        VIMIXFS_BLOCKS_FOR_BITMAP(file->super_block.size) * BLOCK_SIZE;

    file->bitmap_errors = 0;
    for (size_t i = 0; i < bitmap_size; i++)
    {
        if (file->bitmap[i] != file->shadow_bitmap[i])
        {
            file->bitmap_errors += check_bitmap_char(
                file->bitmap[i], file->shadow_bitmap[i], i * 8);
        }
    }

    printf("Bitmap check done, %zd errors\n", file->bitmap_errors);

    printf("%d / %d blocks used\n",
           file->super_block.size - vimixfs_get_free_block_count(file),
           file->super_block.size);
}

int check_file_system(const char* filename, bool verbose)
{
    struct vimixfs file;
    vimixfs_open(&file, filename);

    print_super_block_info(&file);
    print_fs_layout(&file);

    print_log_header(&file);

    check_inodes(&file, verbose);

    check_bitmap(&file);

    file.inode_errors = 0;
    // Inode 0 is not used, start at 1
    for (size_t i = 1;
         i < file.super_block.ninode_blocks * VIMIXFS_INODES_PER_BLOCK; ++i)
    {
        if (!INODE_OK(file.inodes[i]))
        {
            printf("ERROR: Inode %zd ", i);
            if (file.inodes[i] == INODE_DEFINE)
            {
                printf("defined but not referenced in a dir.\n");
            }
            else if (file.inodes[i] == INODE_REFERENCED)
            {
                printf("defined referenced in a dir but not defined.\n");
            }
            file.inode_errors++;
        }
    }
    if (file.inode_errors == 0)
    {
        printf("All existing inodes referenced by dirs.\n");
    }

    int errors = file.inode_errors + file.bitmap_errors;

    vimixfs_close(&file);
    return errors;
}

int main(int argc, char* argv[])
{
    if (argc == 2)
    {
        return check_file_system(argv[1], false);
    }
    fprintf(stderr, "Usage: fsck.vimixfs fs.img\n");
    return 1;
}
