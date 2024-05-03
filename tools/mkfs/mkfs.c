/* SPDX-License-Identifier: MIT */

#include <assert.h>
#include <dirent.h>
#include <fcntl.h>
#include <kernel/xv6fs.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include <linux/limits.h>

#define MAX_ACTIVE_INODESS 200

// Disk layout:
// [ boot block | sb block | log | inode blocks | free bit map | data blocks ]

int ninodeblocks = MAX_ACTIVE_INODESS / IPB + 1;
int nlog = LOGSIZE;
int nmeta;  // Number of meta blocks (boot, g_super_block, nlog, inode, bitmap)
int nblocks;  // Number of data blocks

int g_filesystem_fd;
struct xv6fs_superblock g_super_block;
char zero_block[BLOCK_SIZE] = {0};

/// @brief next free inode number
uint32_t g_freeinode = 1;

uint32_t freeblock;

void balloc(int);
void write_sector(uint32_t, void *);
void write_inode(uint32_t, struct xv6fs_dinode *);
void read_inode(uint32_t inum, struct xv6fs_dinode *ip);
void read_sector(uint32_t sec, void *buf);
uint32_t i_alloc(uint16_t type);
void iappend(uint32_t inum, void *p, int n);
void die(const char *);

/// convert to riscv byte order
uint16_t xshort(uint16_t x)
{
    uint16_t y;
    uint8_t *a = (uint8_t *)&y;
    a[0] = x;
    a[1] = x >> 8;
    return y;
}

/// convert to riscv byte order
uint32_t xint(uint32_t x)
{
    uint32_t y;
    uint8_t *a = (uint8_t *)&y;
    a[0] = x;
    a[1] = x >> 8;
    a[2] = x >> 16;
    a[3] = x >> 24;
    return y;
}

void add_directory_entry(uint32_t inode_new_entry, uint32_t inode_dir,
                         const char *filename)
{
    struct xv6fs_dirent de;
    memset(&de, 0, sizeof(de));

    de.inum = xshort(inode_new_entry);
    strncpy(de.name, filename, XV6_NAME_MAX);
    iappend(inode_dir, &de, sizeof(de));
}

uint32_t create_root_directory()
{
    uint32_t inode = i_alloc(XV6_FT_DIR);
    assert(inode == ROOT_INODE);

    add_directory_entry(ROOT_INODE, ROOT_INODE, ".");
    add_directory_entry(ROOT_INODE, ROOT_INODE, "..");

    return inode;
}

uint32_t create_directory(uint32_t inode_parent, const char *dir_name)
{
    uint32_t inode = i_alloc(XV6_FT_DIR);

    add_directory_entry(inode, inode, ".");
    add_directory_entry(inode_parent, inode, "..");
    add_directory_entry(inode, inode_parent, dir_name);

    return inode;
}

/// @brief Create an empty filesystem (just "." and "..")
/// @param filename output filename of the file system
/// @return root inode number
uint32_t create_empty_filesystem(const char *filename, size_t fs_size)
{
    if (fs_size % BLOCK_SIZE != 0)
    {
        printf("ERROR: file system size must be a multiple of BLOCK_SITE %d\n",
               BLOCK_SIZE);
        exit(-1);
    }
    size_t min_fs_size = (2 + nlog + ninodeblocks + 1) * BLOCK_SIZE;
    if (fs_size < min_fs_size)
    {
        printf("ERROR: min file system size is %ld bytes\n", min_fs_size);
        exit(-1);
    }
    size_t fs_size_in_blocks = fs_size / BLOCK_SIZE;
    int nbitmap = fs_size_in_blocks / (BLOCK_SIZE * 8) + 1;
    nmeta = 2 + nlog + ninodeblocks + nbitmap;

    // open and/or create output file
    g_filesystem_fd = open(filename, O_RDWR | O_CREAT | O_TRUNC, 0666);
    if (g_filesystem_fd < 0)
    {
        die(filename);
    }

    // 1 fs block = 1 disk sector
    nblocks = fs_size_in_blocks - nmeta;

    g_super_block.magic = XV6FS_MAGIC;
    g_super_block.size = xint(fs_size_in_blocks);
    g_super_block.nblocks = xint(nblocks);
    g_super_block.ninodes = xint(MAX_ACTIVE_INODESS);
    g_super_block.nlog = xint(nlog);
    g_super_block.logstart = xint(2);
    g_super_block.inodestart = xint(2 + nlog);
    g_super_block.bmapstart = xint(2 + nlog + ninodeblocks);

    printf(
        "nmeta %d (boot, super, log blocks %u inode blocks %u, bitmap blocks "
        "%u) blocks %d total %ld\n",
        nmeta, nlog, ninodeblocks, nbitmap, nblocks, fs_size_in_blocks);

    freeblock = nmeta;  // the first free block that we can allocate

    // fill file with zeroes
    for (int i = 0; i < fs_size_in_blocks; i++)
    {
        write_sector(i, zero_block);
    }

    char block_buffer[BLOCK_SIZE];
    memset(block_buffer, 0, sizeof(block_buffer));
    memmove(block_buffer, &g_super_block, sizeof(g_super_block));
    write_sector(1, block_buffer);

    uint32_t rootino = create_root_directory();
    return rootino;
}

void fix_dir_size(int32_t inode)
{
    // fix size of dir inode: round up offset to BLOCK_SIZE
    struct xv6fs_dinode din;
    read_inode(inode, &din);
    uint32_t off = xint(din.size);
    if (off % BLOCK_SIZE != 0)
    {
        off = ((off / BLOCK_SIZE) + 1) * BLOCK_SIZE;
        din.size = xint(off);
        write_inode(inode, &din);
    }
}

bool is_dot_or_dotdot(const char *file_name)
{
    if (file_name[0] == '.')
    {
        if (file_name[1] == 0) return true;
        if (file_name[1] == '.' && file_name[2] == 0) return true;
    }

    return false;
}

int build_full_path(char *dst, const char *path, const char *file)
{
    strncpy(dst, path, PATH_MAX);
    size_t len = strlen(dst);
    if (dst[len - 1] != '/')
    {
        dst[len] = '/';
        len++;
    }
    size_t name_len = strlen(file);
    if (len + name_len > PATH_MAX - 1)
    {
        return -1;
    }
    strncpy(dst + len, file, PATH_MAX - len);

    return 0;
}

void copy_file_to_filesystem(const char *path_on_host, const char *new_name,
                             int32_t dir_inode_on_fs)
{
    int fd = open(path_on_host, 0);
    if (fd < 0) die(path_on_host);

    size_t max_file_size = MAXFILE * BLOCK_SIZE;
    struct stat st;
    if (fstat(fd, &st) < 0) return;
    if (st.st_size > max_file_size)
    {
        printf("error: can't copy file %s because it is too big.\n",
               path_on_host);
        printf("File size: %ld, max file size: %ld\n", st.st_size,
               max_file_size);
        return;  // skip file
    }

    uint32_t inum = i_alloc(XV6_FT_FILE);

    add_directory_entry(inum, dir_inode_on_fs, new_name);

    int cc;
    char block_buffer[BLOCK_SIZE];
    while ((cc = read(fd, block_buffer, sizeof(block_buffer))) > 0)
    {
        iappend(inum, block_buffer, cc);
    }

    close(fd);

    fix_dir_size(dir_inode_on_fs);
}

void copy_dir_to_filesystem(const char *dir_on_host, int32_t dir_inode_on_fs)
{
    DIR *dir = opendir(dir_on_host);
    if (dir == NULL)
    {
        return;
    }

    char full_path[PATH_MAX];
    struct dirent *dir_entry = NULL;
    while ((dir_entry = readdir(dir)))
    {
        if (is_dot_or_dotdot(dir_entry->d_name)) continue;

        build_full_path(full_path, dir_on_host, dir_entry->d_name);
        struct stat st;
        if (stat(full_path, &st) < 0) return;

        if (S_ISDIR(st.st_mode))
        {
            uint32_t new_dir =
                create_directory(dir_inode_on_fs, dir_entry->d_name);
            copy_dir_to_filesystem(full_path, new_dir);
        }
        else if (S_ISREG(st.st_mode))
        {
            copy_file_to_filesystem(full_path, dir_entry->d_name,
                                    dir_inode_on_fs);
        }
    }

    closedir(dir);
}

int main(int argc, char *argv[])
{
    _Static_assert(sizeof(int) == 4, "Integers must be 4 bytes!");
    assert((BLOCK_SIZE % sizeof(struct xv6fs_dinode)) == 0);
    assert((BLOCK_SIZE % sizeof(struct xv6fs_dirent)) == 0);

    if (argc != 3)
    {
        fprintf(stderr, "Usage: mkfs fs.img dir\n");
        exit(1);
    }

    uint32_t rootino = create_empty_filesystem(argv[1], 2048 * BLOCK_SIZE);
    copy_dir_to_filesystem(argv[2], rootino);

    balloc(freeblock);

    return 0;
}

/// @brief Write one block to the file at location sec.
/// @param sec sector index
/// @param buf the buffer to read from
void write_sector(uint32_t sec, void *buf)
{
    if (lseek(g_filesystem_fd, sec * BLOCK_SIZE, 0) != sec * BLOCK_SIZE)
        die("lseek");
    if (write(g_filesystem_fd, buf, BLOCK_SIZE) != BLOCK_SIZE) die("write");
}

void write_inode(uint32_t inum, struct xv6fs_dinode *ip)
{
    char buf[BLOCK_SIZE];
    struct xv6fs_dinode *dip;

    uint32_t block_index = IBLOCK(inum, g_super_block);
    read_sector(block_index, buf);
    dip = ((struct xv6fs_dinode *)buf) + (inum % IPB);
    *dip = *ip;  // copy struct of the inode to dip (== copy into buf)
    write_sector(block_index, buf);
}

void read_inode(uint32_t inum, struct xv6fs_dinode *ip)
{
    char buf[BLOCK_SIZE];
    struct xv6fs_dinode *dip;

    uint32_t block_index = IBLOCK(inum, g_super_block);
    read_sector(block_index, buf);
    dip = ((struct xv6fs_dinode *)buf) + (inum % IPB);
    *ip = *dip;  // copy struct into ip
}

/// @brief Reads a sector from the file to the buffer
/// @param sec sector index
/// @param buf destination buffer
void read_sector(uint32_t sec, void *buf)
{
    if (lseek(g_filesystem_fd, sec * BLOCK_SIZE, 0) != sec * BLOCK_SIZE)
        die("lseek");
    if (read(g_filesystem_fd, buf, BLOCK_SIZE) != BLOCK_SIZE) die("read");
}

/// @brief Allocates a new unique inode number and creates a disk inode.
/// @param type type of the new inode
/// @return number of the new inode
uint32_t i_alloc(uint16_t type)
{
    uint32_t inum = g_freeinode++;
    struct xv6fs_dinode din;

    memset(&din, 0, sizeof(din));

    din.type = xshort(type);
    din.nlink = xshort(1);
    din.size = xint(0);
    write_inode(inum, &din);
    return inum;
}

/// @brief Bitmap alloc: prepare blocks for the bitmap
/// @param used
void balloc(int used)
{
    printf("balloc: first %d blocks have been allocated\n", used);
    assert(used < BLOCK_SIZE * 8);

    uint8_t buf[BLOCK_SIZE];
    memset(&buf, 0, BLOCK_SIZE);

    for (int i = 0; i < used; i++)
    {
        buf[i / 8] = buf[i / 8] | (0x1 << (i % 8));
    }
    printf("balloc: write bitmap block at sector %d\n",
           g_super_block.bmapstart);
    write_sector(g_super_block.bmapstart, buf);
}

#define min(a, b) ((a) < (b) ? (a) : (b))

void iappend(uint32_t inum, void *xp, int n)
{
    char *p = (char *)xp;
    uint32_t fbn, off, n1;
    struct xv6fs_dinode din;
    char buf[BLOCK_SIZE];
    uint32_t indirect[NINDIRECT];
    uint32_t x;

    read_inode(inum, &din);
    off = xint(din.size);
    // printf("append inum %d at off %d sz %d\n", inum, off, n);
    while (n > 0)
    {
        fbn = off / BLOCK_SIZE;
        assert(fbn < MAXFILE);
        if (fbn < NDIRECT)
        {
            if (xint(din.addrs[fbn]) == 0)
            {
                din.addrs[fbn] = xint(freeblock++);
            }
            x = xint(din.addrs[fbn]);
        }
        else
        {
            if (xint(din.addrs[NDIRECT]) == 0)
            {
                din.addrs[NDIRECT] = xint(freeblock++);
            }
            read_sector(xint(din.addrs[NDIRECT]), (char *)indirect);
            if (indirect[fbn - NDIRECT] == 0)
            {
                indirect[fbn - NDIRECT] = xint(freeblock++);
                write_sector(xint(din.addrs[NDIRECT]), (char *)indirect);
            }
            x = xint(indirect[fbn - NDIRECT]);
        }
        n1 = min(n, (fbn + 1) * BLOCK_SIZE - off);
        read_sector(x, buf);

        memmove(buf + off - (fbn * BLOCK_SIZE), p, n1);

        write_sector(x, buf);
        n -= n1;
        off += n1;
        p += n1;
    }
    din.size = xint(off);
    write_inode(inum, &din);
}

/// @brief Exit the program after printing a error message
/// @param error_message the message
void die(const char *error_message)
{
    printf("ERROR: %s\n", error_message);
    exit(1);
}
