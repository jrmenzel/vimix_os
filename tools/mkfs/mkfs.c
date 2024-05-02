/* SPDX-License-Identifier: MIT */

#include <assert.h>
#include <fcntl.h>
#include <kernel/xv6fs.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define FSSIZE 2000  ///< size of file system in blocks
#define MAX_ACTIVE_INODESS 200

// Disk layout:
// [ boot block | sb block | log | inode blocks | free bit map | data blocks ]

int nbitmap = FSSIZE / (BLOCK_SIZE * 8) + 1;
int ninodeblocks = MAX_ACTIVE_INODESS / IPB + 1;
int nlog = LOGSIZE;
int nmeta;    // Number of meta blocks (boot, sb, nlog, inode, bitmap)
int nblocks;  // Number of data blocks

int fsfd;
struct xv6fs_superblock sb;
char zeroes[BLOCK_SIZE];
uint32_t freeinode = 1;
uint32_t freeblock;

void balloc(int);
void wsect(uint32_t, void *);
void winode(uint32_t, struct xv6fs_dinode *);
void rinode(uint32_t inum, struct xv6fs_dinode *ip);
void rsect(uint32_t sec, void *buf);
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

int main(int argc, char *argv[])
{
    int i, cc, fd;
    uint32_t rootino, inum, off;
    struct xv6fs_dirent de;
    char buf[BLOCK_SIZE];
    struct xv6fs_dinode din;

    _Static_assert(sizeof(int) == 4, "Integers must be 4 bytes!");

    if (argc < 2)
    {
        fprintf(stderr, "Usage: mkfs fs.img files...\n");
        exit(1);
    }

    assert((BLOCK_SIZE % sizeof(struct xv6fs_dinode)) == 0);
    assert((BLOCK_SIZE % sizeof(struct xv6fs_dirent)) == 0);

    fsfd = open(argv[1], O_RDWR | O_CREAT | O_TRUNC, 0666);
    if (fsfd < 0) die(argv[1]);

    // 1 fs block = 1 disk sector
    nmeta = 2 + nlog + ninodeblocks + nbitmap;
    nblocks = FSSIZE - nmeta;

    sb.magic = XV6FS_MAGIC;
    sb.size = xint(FSSIZE);
    sb.nblocks = xint(nblocks);
    sb.ninodes = xint(MAX_ACTIVE_INODESS);
    sb.nlog = xint(nlog);
    sb.logstart = xint(2);
    sb.inodestart = xint(2 + nlog);
    sb.bmapstart = xint(2 + nlog + ninodeblocks);

    printf(
        "nmeta %d (boot, super, log blocks %u inode blocks %u, bitmap blocks "
        "%u) blocks %d total %d\n",
        nmeta, nlog, ninodeblocks, nbitmap, nblocks, FSSIZE);

    freeblock = nmeta;  // the first free block that we can allocate

    for (i = 0; i < FSSIZE; i++) wsect(i, zeroes);

    memset(buf, 0, sizeof(buf));
    memmove(buf, &sb, sizeof(sb));
    wsect(1, buf);

    rootino = i_alloc(XV6_FT_DIR);
    assert(rootino == ROOT_INODE);

    memset(&de, 0, sizeof(de));

    de.inum = xshort(rootino);
    strcpy(de.name, ".");
    iappend(rootino, &de, sizeof(de));

    memset(&de, 0, sizeof(de));

    de.inum = xshort(rootino);
    strcpy(de.name, "..");
    iappend(rootino, &de, sizeof(de));

    for (i = 2; i < argc; i++)
    {
        // get rid of hardcoded path + pathlength
        char *shortname;
        if (strncmp(argv[i], "build/root/", 11) == 0)
            shortname = argv[i] + 11;
        else
            shortname = argv[i];

        assert(index(shortname, '/') == 0);

        if ((fd = open(argv[i], 0)) < 0) die(argv[i]);

        // Skip leading _ in name when writing to file system.
        // The binaries are named _rm, _cat, etc. to keep the
        // build operating system from trying to execute them
        // in place of system binaries like rm and cat.
        if (shortname[0] == '_') shortname += 1;

        inum = i_alloc(XV6_FT_FILE);

        memset(&de, 0, sizeof(de));

        de.inum = xshort(inum);
        strncpy(de.name, shortname, XV6_NAME_MAX);
        iappend(rootino, &de, sizeof(de));

        while ((cc = read(fd, buf, sizeof(buf))) > 0) iappend(inum, buf, cc);

        close(fd);
    }

    // fix size of root inode dir
    rinode(rootino, &din);
    off = xint(din.size);
    off = ((off / BLOCK_SIZE) + 1) * BLOCK_SIZE;
    din.size = xint(off);
    winode(rootino, &din);

    balloc(freeblock);

    exit(0);
}

void wsect(uint32_t sec, void *buf)
{
    if (lseek(fsfd, sec * BLOCK_SIZE, 0) != sec * BLOCK_SIZE) die("lseek");
    if (write(fsfd, buf, BLOCK_SIZE) != BLOCK_SIZE) die("write");
}

void winode(uint32_t inum, struct xv6fs_dinode *ip)
{
    char buf[BLOCK_SIZE];
    uint32_t bn;
    struct xv6fs_dinode *dip;

    bn = IBLOCK(inum, sb);
    rsect(bn, buf);
    dip = ((struct xv6fs_dinode *)buf) + (inum % IPB);
    *dip = *ip;
    wsect(bn, buf);
}

void rinode(uint32_t inum, struct xv6fs_dinode *ip)
{
    char buf[BLOCK_SIZE];
    uint32_t bn;
    struct xv6fs_dinode *dip;

    bn = IBLOCK(inum, sb);
    rsect(bn, buf);
    dip = ((struct xv6fs_dinode *)buf) + (inum % IPB);
    *ip = *dip;
}

void rsect(uint32_t sec, void *buf)
{
    if (lseek(fsfd, sec * BLOCK_SIZE, 0) != sec * BLOCK_SIZE) die("lseek");
    if (read(fsfd, buf, BLOCK_SIZE) != BLOCK_SIZE) die("read");
}

uint32_t i_alloc(uint16_t type)
{
    uint32_t inum = freeinode++;
    struct xv6fs_dinode din;

    memset(&din, 0, sizeof(din));

    din.type = xshort(type);
    din.nlink = xshort(1);
    din.size = xint(0);
    winode(inum, &din);
    return inum;
}

void balloc(int used)
{
    uint8_t buf[BLOCK_SIZE];
    int i;

    printf("balloc: first %d blocks have been allocated\n", used);
    assert(used < BLOCK_SIZE * 8);

    memset(&buf, 0, BLOCK_SIZE);

    for (i = 0; i < used; i++)
    {
        buf[i / 8] = buf[i / 8] | (0x1 << (i % 8));
    }
    printf("balloc: write bitmap block at sector %d\n", sb.bmapstart);
    wsect(sb.bmapstart, buf);
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

    rinode(inum, &din);
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
            rsect(xint(din.addrs[NDIRECT]), (char *)indirect);
            if (indirect[fbn - NDIRECT] == 0)
            {
                indirect[fbn - NDIRECT] = xint(freeblock++);
                wsect(xint(din.addrs[NDIRECT]), (char *)indirect);
            }
            x = xint(indirect[fbn - NDIRECT]);
        }
        n1 = min(n, (fbn + 1) * BLOCK_SIZE - off);
        rsect(x, buf);

        memmove(buf + off - (fbn * BLOCK_SIZE), p, n1);

        wsect(x, buf);
        n -= n1;
        off += n1;
        p += n1;
    }
    din.size = xint(off);
    winode(inum, &din);
}

void die(const char *s)
{
    printf("ERROR: %s\n", s);
    exit(1);
}
