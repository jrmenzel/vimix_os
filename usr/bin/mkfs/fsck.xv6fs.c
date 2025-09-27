/* SPDX-License-Identifier: MIT */

#include <errno.h>
#include <fcntl.h>
#include <kernel/xv6fs.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

struct xv6fs_in_file
{
    int fd;

    // super block of opened file system:
    struct xv6fs_superblock super_block;

    // log header of the opened file system:
    struct xv6fs_log_header log_header;

    // to build a bitmap of used blocks and compare it to the bitmap of the fs
    // later:
    uint8_t *bitmap;
    size_t bitmap_errors;

    // store for each inore if it is in use and if it is referenced by a
    // directory:
#define INODE_UNUSED 0
#define INODE_REFERENCED 1
#define INODE_DEFINE 2
#define INODE_OK(x) \
    ((x == INODE_UNUSED) || (x == (INODE_REFERENCED & INODE_DEFINE)))
    uint8_t *inodes;
    size_t inode_errors;

} g_fs_file;

void read_block(struct xv6fs_in_file *file, size_t block_id, uint8_t *buffer)
{
    off_t offset = lseek(file->fd, block_id * BLOCK_SIZE, SEEK_SET);
    if (offset < 0)
    {
        fprintf(stderr, "File ended unexpectedly (errno: %s)\n\n",
                strerror(errno));
        exit(1);
    }
    ssize_t r = read(file->fd, buffer, BLOCK_SIZE);
    if (r != BLOCK_SIZE)
    {
        fprintf(stderr, "File ended unexpectedly (errno: %s)\n\n",
                strerror(errno));
        exit(1);
    }
}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wanalyzer-use-of-uninitialized-value"
void mark_block_as_used(struct xv6fs_in_file *file, uint32_t addr)
{
    if (file == NULL || file->bitmap == NULL) return;

    // the offset is 0 instead of bmapstart as the bitmap is the array g_bitmap
    // and not the in-file bitmap
    size_t block = XV6FS_BMAP_BLOCK_OF_BIT(addr, 0);
    uint8_t *bitmap_block = file->bitmap + (BLOCK_SIZE * block);
    int32_t local_addr = addr % XV6FS_BMAP_BITS_PER_BLOCK;

    uint8_t bit_to_set = 1 << (local_addr % 8);
    local_addr = local_addr / 8;

    if ((bitmap_block[local_addr] & bit_to_set) != 0)
    {
        printf("Error: block is in use multiple times %d\n", addr);
    }

    bitmap_block[local_addr] = bitmap_block[local_addr] | bit_to_set;
}
#pragma GCC diagnostic pop

void print_fs_layout(struct xv6fs_in_file *file)
{
    struct xv6fs_superblock *sb = &file->super_block;
    size_t blocks_for_bitmap = XV6_BLOCKS_FOR_BITMAP(sb->size);
    size_t blocks_for_inodes = (sb->ninodes / XV6FS_INODES_PER_BLOCK) + 1;

    printf("Blocks:\n");
    printf("[0]         = Reserved for boot loader\n");
    printf("[1]         = Superblock\n");
    printf("[%d]         = Log Header\n", sb->logstart);
    printf("[%d] - [%d]  = Log Entries\n", sb->logstart + 1,
           sb->logstart + sb->nlog - 1);
    printf("[%d] - [%zd] = Inodes (%zd inodes per block)\n", sb->inodestart,
           sb->inodestart + blocks_for_inodes - 1, XV6FS_INODES_PER_BLOCK);
    size_t bmap_end = sb->bmapstart + blocks_for_bitmap - 1;
    if (blocks_for_bitmap == 1)
    {
        printf("[%d]        = Bitmap\n", sb->bmapstart);
    }
    else
    {
        printf("[%d] - [%zd] = Bitmap\n", sb->bmapstart, bmap_end);
    }
    size_t data_start = bmap_end + 1;
    printf("[%zd] - [%zd] = Data\n", data_start, data_start + sb->nblocks);

    for (uint32_t b = 0; b < data_start; ++b)
    {
        mark_block_as_used(file, b);
    }

    if (sb->size != data_start + sb->nblocks)
    {
        printf("FS size error\n");
    }
}

void print_super_block_info(struct xv6fs_in_file *file)
{
    struct xv6fs_superblock *sb = &file->super_block;
    printf("Super Block\n");
    printf("Magic: %x (expected: %x)\n", sb->magic, XV6FS_MAGIC);
    printf("Size:         %d\n", sb->size);
    printf("Data Blocks:  %d\n", sb->nblocks);
    printf("Max Inodes:   %d\n", sb->ninodes);
    printf("Log Blocks:   %d\n", sb->nlog);
    printf("Log Start:    %d\n", sb->logstart);
    printf("Inode Start:  %d\n", sb->inodestart);
    printf("Bitmap Start: %d\n", sb->bmapstart);
}

void print_log_header(struct xv6fs_in_file *file)
{
    struct xv6fs_log_header *log = &file->log_header;
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

void check_dirents(struct xv6fs_in_file *file, uint32_t addr)
{
    uint8_t buf[BLOCK_SIZE];
    read_block(file, addr, buf);
    struct xv6fs_dirent *dir = (struct xv6fs_dirent *)buf;

    size_t dirents_per_block = BLOCK_SIZE % sizeof(struct xv6fs_dirent);
    for (size_t i = 0; i < dirents_per_block; ++i)
    {
        size_t inum = dir[i].inum;
        file->inodes[inum] = file->inodes[inum] & INODE_REFERENCED;
    }
}

// returns 1 if the disk inode is in use, 0 otherwise
int check_dinode(struct xv6fs_in_file *file, struct xv6fs_dinode *dinode,
                 size_t inum, bool verbose)
{
    if (file == NULL || file->inodes == NULL || dinode == NULL)
    {
        printf("ERROR in check_dinode\n");
        exit(1);
    }

    if (dinode->type == XV6_FT_UNUSED)
    {
        file->inodes[inum] = INODE_UNUSED;
        return 0;
    }

    if (verbose)
        printf("%zd (block %zd, %zd) ", inum, (inum / XV6FS_INODES_PER_BLOCK),
               inum % XV6FS_INODES_PER_BLOCK);

    switch (dinode->type)
    {
        case XV6_FT_DIR:
            if (verbose) printf("dir");
            break;
        case XV6_FT_FILE:
            if (verbose) printf("file");
            break;
        case XV6_FT_CHAR_DEVICE:
            if (verbose) printf("c dev (%d,%d)", dinode->major, dinode->minor);
            break;
        case XV6_FT_BLOCK_DEVICE:
            if (verbose) printf("b dev (%d,%d)", dinode->major, dinode->minor);
            break;
        default: printf("UNKNOWN inode type %d\n", dinode->type); return 0;
    }
    file->inodes[inum] = file->inodes[inum] & INODE_DEFINE;

    if (dinode->type == XV6_FT_DIR || dinode->type == XV6_FT_FILE)
    {
        for (size_t i = 0; i < XV6FS_N_DIRECT_BLOCKS; ++i)
        {
            if (dinode->addrs[i] != 0)
            {
                mark_block_as_used(file, dinode->addrs[i]);
                check_dirents(file, dinode->addrs[i]);
                if (verbose) printf(" [%d]", dinode->addrs[i]);
            }
        }
        if (dinode->addrs[XV6FS_N_DIRECT_BLOCKS] != 0)
        {
            mark_block_as_used(file, dinode->addrs[XV6FS_N_DIRECT_BLOCKS]);
            uint8_t *buffer = malloc(BLOCK_SIZE);
            if (buffer == NULL)
            {
                printf("ERROR: out of memory\n");
                exit(1);
            }
            read_block(file, dinode->addrs[XV6FS_N_DIRECT_BLOCKS], buffer);
            uint32_t *addr = (uint32_t *)buffer;
            for (size_t i = 0; i < BLOCK_SIZE / sizeof(uint32_t); ++i)
            {
                if (addr[i] != 0)
                {
                    mark_block_as_used(file, addr[i]);
                    check_dirents(file, addr[i]);
                    if (verbose) printf(" [%d]", addr[i]);
                }
            }
            free(buffer);
        }
    }

    if (verbose) printf("\n");

    return 1;
}

void check_inodes(struct xv6fs_in_file *file, bool verbose)
{
    struct xv6fs_superblock *sb = &file->super_block;
    uint8_t buffer[BLOCK_SIZE];
    size_t used = 0;
    ino_t inum = INVALID_INODE;
    size_t inode_end = (sb->ninodes / XV6FS_INODES_PER_BLOCK) + sb->inodestart;
    for (size_t block = sb->inodestart; block < inode_end; ++block)
    {
        read_block(file, block, buffer);
        struct xv6fs_dinode *dinode = (struct xv6fs_dinode *)buffer;
        for (size_t i = 0; i < XV6FS_INODES_PER_BLOCK; ++i)
        {
            used += check_dinode(file, &dinode[i], inum, verbose);
            inum++;
        }
    }
    printf("%zd disk inodes used (of %d)\n", used, sb->ninodes);
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

size_t check_bitmap_block(uint8_t *bm_file, uint8_t *bm_calculated,
                          size_t offset)
{
    size_t errors = 0;
    for (size_t i = 0; i < BLOCK_SIZE; ++i)
    {
        if (bm_file[i] != bm_calculated[i])
        {
            errors +=
                check_bitmap_char(bm_file[i], bm_calculated[i], offset + i * 8);
        }
    }
    return errors;
}

void check_bitmap(struct xv6fs_in_file *file)
{
    uint8_t buffer[BLOCK_SIZE];
    size_t blocks = XV6_BLOCKS_FOR_BITMAP(file->super_block.size);

    file->bitmap_errors = 0;
    for (size_t i = 0; i < blocks; ++i)
    {
        read_block(file, i + file->super_block.bmapstart, buffer);
        file->bitmap_errors +=
            check_bitmap_block(buffer, file->bitmap + (BLOCK_SIZE * i),
                               i * XV6FS_BMAP_BITS_PER_BLOCK);
    }

    printf("Bitmap check done, %zd errors\n", file->bitmap_errors);
}

int check_file_system(int fd, bool verbose)
{
    struct xv6fs_in_file file;
    file.fd = fd;

    uint8_t buffer[BLOCK_SIZE];
    read_block(&file, 1, buffer);
    memcpy(&file.super_block, buffer, sizeof(struct xv6fs_superblock));
    file.bitmap =
        malloc(BLOCK_SIZE * XV6_BLOCKS_FOR_BITMAP(file.super_block.size));
    file.inodes = malloc(file.super_block.ninodes);
    if (file.inodes == NULL)
    {
        printf("ERROR: out of memory\n");
        exit(1);
    }
    for (size_t i = 0; i < file.super_block.ninodes; ++i)
    {
        file.inodes[i] = INODE_UNUSED;
    }

    print_super_block_info(&file);
    print_fs_layout(&file);

    read_block(&file, file.super_block.logstart, buffer);
    memcpy(&file.log_header, buffer, sizeof(struct xv6fs_log_header));

    print_log_header(&file);

    check_inodes(&file, verbose);

    check_bitmap(&file);

    file.inode_errors = 0;
    // Inode 0 is not used, start at 1
    for (size_t i = 1; i < file.super_block.ninodes; ++i)
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

    free(file.inodes);
    free(file.bitmap);
    return (file.inode_errors + file.bitmap_errors);
}

int main(int argc, char *argv[])
{
    if (argc == 2)
    {
        int fd = open(argv[1], O_RDWR);
        if (fd > 0)
        {
            int ret = check_file_system(fd, false);
            close(fd);
            return ret;
        }
        fprintf(stderr, "Could not open file %s\n", argv[1]);
        return 1;
    }
    fprintf(stderr, "Usage: fsck.xv6fs fs.img\n");
    return 1;
}
